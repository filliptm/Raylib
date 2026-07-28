#!/usr/bin/env python3
"""Repack a Meshy AI character export into a raylib-loadable GLB.

Why this exists
---------------
raylib 5.5's glTF loader has a narrower contract than the glTF spec:

  1. It IGNORES the skin's inverseBindMatrices. The bind pose is rebuilt from each
     joint node's world transform, so the node rest pose must BE the bind pose.
  2. Animation frame poses are composed through bone parents only - transforms on
     nodes above the skeleton root (Meshy's 'Armature' node, scale 0.01) are
     excluded, while the bind pose includes them. Any such transform desyncs the two.
  3. Unkeyed animation channels fall back to the node's static TRS.
  4. BuildPoseFromParentJoints does not apply parent scale to child translations,
     so joint local scales in animations must stay ~1.

Meshy exports violate 1 and 2: the mesh is stored at ~1/100 scale, the real bind
lives in the inverseBindMatrices (which carry a 100x scale), and the Armature node
holds scale 0.01. raylib renders that as a collapsed spike-ball.

What this does
--------------
  - Rewrites every joint's static TRS so the node hierarchy composes to
    k * inverse(inverseBindMatrix), where k cancels the uniform scale hidden in the
    IBMs. That puts the bind skeleton in the same space as the animation channels
    with all bind scales == 1, which matters because raylib's TRS inverse in
    UpdateModelAnimationBones drops 1/scale on the translation and its final
    compose multiplies translation by scale - both wrong unless scale is 1.
  - Bakes the same k into the mesh vertex positions so the skin still matches.
  - Strips the transform from every non-joint node (Armature, mesh node).
  - Because Meshy keys translation+rotation+scale on every joint, animations never
    read the statics; where a channel IS missing, a constant channel holding the
    original static value is inserted so the rewrite cannot leak into animation.
  - Merges the separate per-animation GLBs into one file with named clips.
  - Standardizes every embedded character texture to the runtime 1024x1024 contract.

Usage:  python3 fix_meshy_glb.py <dir-with-meshy-glbs> <output.glb> [texture-size]
Both Meshy export styles are handled: one GLB per animation (clip named from its
filename) and the newer single merged-animations GLB (each clip keeps its own name,
lowercased). The file carrying the most clips becomes the base; a T-pose
Character_output file just contributes its clip.
"""
import struct, json, math, io, os, re, sys

CHARACTER_TEXTURE_SIZE = 1024

# ---------------------------------------------------------------- glb container

def parse_glb(path):
    d = open(path, 'rb').read()
    magic, ver, _ = struct.unpack('<III', d[:12])
    assert magic == 0x46546C67 and ver == 2, f'not a glTF2 GLB: {path}'
    off, j, bin_ = 12, None, b''
    while off < len(d):
        clen, = struct.unpack('<I', d[off:off+4])
        ctype = d[off+4:off+8]
        payload = d[off+8:off+8+clen]
        if ctype == b'JSON': j = json.loads(payload)
        elif ctype == b'BIN\x00': bin_ = payload
        off += 8 + clen
    return j, bin_

def write_glb(path, j, blob):
    js = json.dumps(j, separators=(',', ':')).encode()
    while len(js) % 4: js += b' '
    blob = bytearray(blob)
    while len(blob) % 4: blob.append(0)
    out = b'glTF' + struct.pack('<II', 2, 12 + 8 + len(js) + 8 + len(blob))
    out += struct.pack('<I', len(js)) + b'JSON' + js
    out += struct.pack('<I', len(blob)) + b'BIN\x00' + bytes(blob)
    open(path, 'wb').write(out)
    return len(out)

# ---------------------------------------------------------------- 4x4 matrices
# Flat column-major (glTF layout): m[col*4 + row].

def mat_mul(A, B):
    """A*B: apply B first, then A."""
    C = [0.0]*16
    for c in range(4):
        for r in range(4):
            C[c*4+r] = sum(A[k*4+r]*B[c*4+k] for k in range(4))
    return C

def mat_inv(M):
    A = [[M[c*4+r] for c in range(4)] for r in range(4)]
    I = [[1.0 if i == j else 0.0 for j in range(4)] for i in range(4)]
    for col in range(4):
        p = max(range(col, 4), key=lambda r: abs(A[r][col]))
        A[col], A[p] = A[p], A[col]
        I[col], I[p] = I[p], I[col]
        d = A[col][col]
        A[col] = [x/d for x in A[col]]
        I[col] = [x/d for x in I[col]]
        for r in range(4):
            if r != col and A[r][col] != 0.0:
                f = A[r][col]
                A[r] = [a - f*b for a, b in zip(A[r], A[col])]
                I[r] = [a - f*b for a, b in zip(I[r], I[col])]
    return [I[r][c] for c in range(4) for r in range(4)]

def trs_to_mat(t, q, s):
    x, y, z, w = q
    r = [1-2*(y*y+z*z), 2*(x*y+z*w),   2*(x*z-y*w),
         2*(x*y-z*w),   1-2*(x*x+z*z), 2*(y*z+x*w),
         2*(x*z+y*w),   2*(y*z-x*w),   1-2*(x*x+y*y)]   # three columns
    return [r[0]*s[0], r[1]*s[0], r[2]*s[0], 0.0,
            r[3]*s[1], r[4]*s[1], r[5]*s[1], 0.0,
            r[6]*s[2], r[7]*s[2], r[8]*s[2], 0.0,
            t[0], t[1], t[2], 1.0]

def mat_to_trs(M):
    t = [M[12], M[13], M[14]]
    s = [math.sqrt(M[0]**2 + M[1]**2 + M[2]**2),
         math.sqrt(M[4]**2 + M[5]**2 + M[6]**2),
         math.sqrt(M[8]**2 + M[9]**2 + M[10]**2)]
    r = [[M[0]/s[0], M[4]/s[1], M[8]/s[2]],
         [M[1]/s[0], M[5]/s[1], M[9]/s[2]],
         [M[2]/s[0], M[6]/s[1], M[10]/s[2]]]
    det = (r[0][0]*(r[1][1]*r[2][2] - r[1][2]*r[2][1])
         - r[0][1]*(r[1][0]*r[2][2] - r[1][2]*r[2][0])
         + r[0][2]*(r[1][0]*r[2][1] - r[1][1]*r[2][0]))
    if det < 0:
        s[0] = -s[0]
        for i in range(3): r[i][0] = -r[i][0]
    tr = r[0][0] + r[1][1] + r[2][2]
    if tr > 0:
        S = math.sqrt(tr + 1.0)*2
        q = [(r[2][1]-r[1][2])/S, (r[0][2]-r[2][0])/S, (r[1][0]-r[0][1])/S, S/4]
    elif r[0][0] > r[1][1] and r[0][0] > r[2][2]:
        S = math.sqrt(1.0 + r[0][0] - r[1][1] - r[2][2])*2
        q = [S/4, (r[0][1]+r[1][0])/S, (r[0][2]+r[2][0])/S, (r[2][1]-r[1][2])/S]
    elif r[1][1] > r[2][2]:
        S = math.sqrt(1.0 + r[1][1] - r[0][0] - r[2][2])*2
        q = [(r[0][1]+r[1][0])/S, S/4, (r[1][2]+r[2][1])/S, (r[0][2]-r[2][0])/S]
    else:
        S = math.sqrt(1.0 + r[2][2] - r[0][0] - r[1][1])*2
        q = [(r[0][2]+r[2][0])/S, (r[1][2]+r[2][1])/S, S/4, (r[1][0]-r[0][1])/S]
    n = math.sqrt(sum(c*c for c in q))
    return t, [c/n for c in q], s

def node_local_mat(node):
    if 'matrix' in node: return list(node['matrix'])
    return trs_to_mat(node.get('translation', [0, 0, 0]),
                      node.get('rotation', [0, 0, 0, 1]),
                      node.get('scale', [1, 1, 1]))

# ---------------------------------------------------------------- main

def main(src_dir, out_path, tex_size=CHARACTER_TEXTURE_SIZE):
    if tex_size != CHARACTER_TEXTURE_SIZE:
        raise ValueError(
            'runtime character textures must be exactly %dx%d, got %d'
            % (CHARACTER_TEXTURE_SIZE, CHARACTER_TEXTURE_SIZE, tex_size)
        )

    glbs = sorted(f for f in os.listdir(src_dir) if f.lower().endswith('.glb'))

    # Newer Meshy exports ship one merged-animations GLB alongside the T-pose file.
    # Whichever file carries the most clips becomes the base (its mesh, skin, texture
    # and animations are internally consistent); everything else contributes clips.
    def anim_count(f):
        jj, _ = parse_glb(os.path.join(src_dir, f))
        return len(jj.get('animations', []))
    counts = {f: anim_count(f) for f in glbs}
    base_name = max(glbs, key=lambda f: (counts[f], 'Character_output' in f))
    clip_files = [f for f in glbs if f != base_name]
    print('base file: %s (%d clips)' % (base_name, counts[base_name]))

    j, bin_ = parse_glb(os.path.join(src_dir, base_name))
    nodes = j['nodes']
    skin = j['skins'][0]
    joints = skin['joints']
    joint_set = set(joints)

    # Rigs must be identical across the per-animation files or merging clips is invalid.
    for f in clip_files:
        cj, _ = parse_glb(os.path.join(src_dir, f))
        assert [n.get('name') for n in cj['nodes']] == [n.get('name') for n in nodes], f'rig differs: {f}'

    views = [dict(v) for v in j['bufferViews']]
    def view_bytes(src_j, src_b, idx):
        v = src_j['bufferViews'][idx]
        s = v.get('byteOffset', 0)
        return bytearray(src_b[s:s+v['byteLength']])
    data = [view_bytes(j, bin_, i) for i in range(len(views))]

    def read_accessor_floats(src_j, src_b, idx):
        a = src_j['accessors'][idx]
        comp = {'SCALAR': 1, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}[a['type']]
        v = src_j['bufferViews'][a['bufferView']]
        off = v.get('byteOffset', 0) + a.get('byteOffset', 0)
        return list(struct.unpack_from('<%df' % (a['count']*comp), src_b, off)), a['count'], comp

    # ---- 1. bind pose that raylib will accept -----------------------------------
    # raylib only skins correctly when every bind-pose scale is 1, so fold the
    # uniform scale the IBMs carry into k, put k*inverse(IBM) into the nodes, and
    # bake k into the vertices below.
    ibms, _, _ = read_accessor_floats(j, bin_, skin['inverseBindMatrices'])

    def linear_scale(m):
        return (math.sqrt(m[0]**2+m[1]**2+m[2]**2) +
                math.sqrt(m[4]**2+m[5]**2+m[6]**2) +
                math.sqrt(m[8]**2+m[9]**2+m[10]**2))/3.0

    k_bake = sum(linear_scale(ibms[i*16:(i+1)*16]) for i in range(len(joints)))/len(joints)
    print('uniform scale folded out of the IBMs: k = %.4f' % k_bake)
    K = [k_bake,0,0,0, 0,k_bake,0,0, 0,0,k_bake,0, 0,0,0,1]

    W = {}                                        # joint node id -> desired world
    for k, node_id in enumerate(joints):
        W[node_id] = mat_mul(K, mat_inv(ibms[k*16:(k+1)*16]))

    parent_of = {}
    for i, n in enumerate(nodes):
        for c in n.get('children', []): parent_of[c] = i

    # Original statics, kept for filler channels before the rewrite below.
    orig_static = {i: (nodes[i].get('translation', [0, 0, 0]),
                       nodes[i].get('rotation', [0, 0, 0, 1]),
                       nodes[i].get('scale', [1, 1, 1])) for i in range(len(nodes))}

    worst_recon = 0.0
    for node_id in joints:                        # joints are parent-first in Meshy files
        p = parent_of.get(node_id)
        L = mat_mul(mat_inv(W[p]), W[node_id]) if p in joint_set else W[node_id]
        t, q, s = mat_to_trs(L)
        worst_recon = max(worst_recon, max(abs(a-b) for a, b in zip(trs_to_mat(t, q, s), L)))
        nd = nodes[node_id]
        nd.pop('matrix', None)
        nd['translation'], nd['rotation'], nd['scale'] = t, q, s

    # Non-joint nodes (Armature wrapper, mesh node) must contribute nothing.
    for i, n in enumerate(nodes):
        if i not in joint_set:
            for k in ('translation', 'rotation', 'scale', 'matrix'): n.pop(k, None)

    # Verify: recompose through the hierarchy and compare against inverse(IBM).
    def world_of(i):
        m = node_local_mat(nodes[i])
        return mat_mul(world_of(parent_of[i]), m) if i in parent_of else m
    worst_world = max(max(abs(a-b) for a, b in zip(world_of(nid), W[nid])) for nid in joints)
    print('rewrote %d joint statics: TRS reconstruction err %.2e, world-vs-invIBM err %.2e'
          % (len(joints), worst_recon, worst_world))

    # ---- 1b. bake k into every mesh position accessor ---------------------------
    seen = set()
    for mesh in j.get('meshes', []):
        for prim in mesh['primitives']:
            pa_idx = prim['attributes']['POSITION']
            if pa_idx in seen: continue
            seen.add(pa_idx)
            pa = j['accessors'][pa_idx]
            v = j['bufferViews'][pa['bufferView']]
            buf = data[pa['bufferView']]
            off = pa.get('byteOffset', 0)
            for kk in range(pa['count']):
                o = off + kk*12
                x, y, z = struct.unpack_from('<3f', buf, o)
                struct.pack_into('<3f', buf, o, x*k_bake, y*k_bake, z*k_bake)
            if 'min' in pa: pa['min'] = [c*k_bake for c in pa['min']]
            if 'max' in pa: pa['max'] = [c*k_bake for c in pa['max']]
            print('baked k into %d vertex positions' % pa['count'])

    # ---- 2. merge animation clips ---------------------------------------------
    def clone_accessor(src_j, src_b, idx):
        a = dict(src_j['accessors'][idx])
        comp = {'SCALAR': 1, 'VEC3': 3, 'VEC4': 4}[a['type']]
        v = src_j['bufferViews'][a['bufferView']]
        off = v.get('byteOffset', 0) + a.get('byteOffset', 0)
        raw = src_b[off:off + a['count']*comp*4]
        views.append({'byteLength': len(raw)})
        data.append(bytearray(raw))
        a['bufferView'] = len(views) - 1
        a.pop('byteOffset', None)
        j['accessors'].append(a)
        return len(j['accessors']) - 1

    def push_floats(vals):
        raw = struct.pack('<%df' % len(vals), *vals)
        views.append({'byteLength': len(raw)})
        data.append(bytearray(raw))
        return len(views) - 1

    def add_filler_channels(anim, duration):
        """Constant channels for any unkeyed joint path, holding the ORIGINAL static,
        so the bind-pose rewrite can never bleed into an animation."""
        keyed = {(c['target']['node'], c['target']['path']) for c in anim['channels']}
        added = 0
        for node_id in joints:
            for path, vals in (('translation', orig_static[node_id][0]),
                               ('rotation',    orig_static[node_id][1]),
                               ('scale',       orig_static[node_id][2])):
                if (node_id, path) in keyed: continue
                tv = push_floats([0.0, duration])
                j['accessors'].append({'bufferView': tv, 'componentType': 5126, 'count': 2,
                                       'type': 'SCALAR', 'min': [0.0], 'max': [duration]})
                t_acc = len(j['accessors']) - 1
                vv = push_floats(list(vals)*2)
                j['accessors'].append({'bufferView': vv, 'componentType': 5126, 'count': 2,
                                       'type': 'VEC4' if path == 'rotation' else 'VEC3'})
                v_acc = len(j['accessors']) - 1
                anim['samplers'].append({'input': t_acc, 'output': v_acc, 'interpolation': 'LINEAR'})
                anim['channels'].append({'sampler': len(anim['samplers']) - 1,
                                         'target': {'node': node_id, 'path': path}})
                added += 1
        return added

    def clip_duration(src_j, src_b, anim):
        dur = 0.0
        for smp in anim['samplers']:
            acc = src_j['accessors'][smp['input']]
            if 'max' in acc: dur = max(dur, acc['max'][0])
            else:
                vals, _, _ = read_accessor_floats(src_j, src_b, smp['input'])
                dur = max(dur, max(vals))
        return dur

    def clip_name(raw):
        n = (raw or 'clip').strip().lower()
        if '|' in n:                            # 'Armature|clip0|baselayer'
            parts = n.split('|')
            n = parts[1] if len(parts) > 1 else parts[0]
        return 'tpose' if n in ('clip0', 'baselayer', 'clip') else n

    used_names = set()
    def unique(n):
        m, k = n, 2
        while m in used_names: m, k = '%s_%d' % (n, k), k + 1
        used_names.add(m)
        return m

    for an in j['animations']:
        an['name'] = unique(clip_name(an.get('name')))
        add_filler_channels(an, clip_duration(j, bin_, an))

    for f in clip_files:
        cj, cb = parse_glb(os.path.join(src_dir, f))
        m = re.search(r'Animation_(.+?)(?:_\d+)?_withSkin', f)
        file_name = (m.group(1) if m else os.path.splitext(f)[0]).lower()
        multi = len(cj.get('animations', [])) > 1
        for an in cj.get('animations', []):
            # A single-clip file is named from its filename (the old per-animation
            # export style); a multi-clip file keeps each animation's own name.
            merged = {'name': unique(clip_name(an.get('name')) if multi else file_name),
                      'samplers': [{'input': clone_accessor(cj, cb, s['input']),
                                    'output': clone_accessor(cj, cb, s['output']),
                                    'interpolation': s.get('interpolation', 'LINEAR')}
                                   for s in an['samplers']],
                      'channels': [{'sampler': c['sampler'], 'target': dict(c['target'])}
                                   for c in an['channels']]}
            filled = add_filler_channels(merged, clip_duration(cj, cb, an))
            j['animations'].append(merged)
            print('merged clip %-34s (%d channels, %d filler)' %
                  (merged['name'], len(merged['channels']), filled))

    # ---- 3. texture: normalize every embedded payload to the 1K contract --------
    try:
        from PIL import Image
    except ImportError as exc:
        raise RuntimeError(
            'Pillow is required to enforce the 1024x1024 character texture contract'
        ) from exc

    for img in j.get('images', []):
        iv = img['bufferView']
        pic = Image.open(io.BytesIO(bytes(data[iv]))).convert('RGB')
        if pic.size != (tex_size, tex_size):
            buf = io.BytesIO()
            pic.resize((tex_size, tex_size), Image.LANCZOS).save(buf, 'PNG', optimize=True)
            print('texture %s -> %dx%d (%.1f MB -> %.2f MB)' %
                  (pic.size, tex_size, tex_size, len(data[iv])/1048576, buf.tell()/1048576))
            data[iv] = bytearray(buf.getvalue())
            views[iv] = {'byteLength': len(data[iv])}
        else:
            print('texture %s already satisfies the %dx%d contract' %
                  (pic.size, tex_size, tex_size))

    # ---- 4. rebuild the buffer and write ---------------------------------------
    blob = bytearray()
    for v, raw in zip(views, data):
        while len(blob) % 4: blob.append(0)
        v['byteOffset'] = len(blob)
        v['byteLength'] = len(raw)
        v['buffer'] = 0
        blob += raw
    j['bufferViews'] = views
    j['buffers'] = [{'byteLength': len(blob)}]

    size = write_glb(out_path, j, blob)
    print('wrote %s (%.2f MB), clips: %s' %
          (out_path, size/1048576, [a['name'] for a in j['animations']]))

if __name__ == '__main__':
    if len(sys.argv) not in (3, 4):
        sys.exit('usage: fix_meshy_glb.py <meshy-export-dir> <output.glb> [texture-size]')
    requested_size = int(sys.argv[3]) if len(sys.argv) > 3 else CHARACTER_TEXTURE_SIZE
    if requested_size != CHARACTER_TEXTURE_SIZE:
        sys.exit(
            'character texture size is fixed at %d; do not generate 512px runtime assets'
            % CHARACTER_TEXTURE_SIZE
        )
    main(sys.argv[1], sys.argv[2], requested_size)
