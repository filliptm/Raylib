# Audio Programming in Raylib

This guide covers all aspects of audio programming with Raylib, from basic sound playback to advanced audio processing.

## Table of Contents
1. [Audio Fundamentals](#audio-fundamentals)
2. [Audio Device Management](#audio-device-management)
3. [Loading and Playing Sounds](#loading-and-playing-sounds)
4. [Music Streaming](#music-streaming)
5. [Audio Streams](#audio-streams)
6. [3D Audio](#3d-audio)
7. [Audio Effects](#audio-effects)
8. [Audio Processing](#audio-processing)
9. [Real-time Audio](#real-time-audio)
10. [Best Practices](#best-practices)

## Audio Fundamentals

### Basic Concepts
- **Wave**: Raw audio data in memory
- **Sound**: Audio loaded into memory for quick playback
- **Music**: Audio streamed from file for large files
- **AudioStream**: Raw audio stream for real-time processing

### Supported Formats
- WAV (uncompressed)
- OGG (compressed, recommended)
- MP3 (compressed)
- FLAC (lossless)
- XM (tracker music)
- MOD (tracker music)

### Basic Audio Setup
```c
#include "raylib.h"

int main(void)
{
    InitWindow(800, 450, "raylib - audio example");
    
    // Initialize audio device
    InitAudioDevice();
    
    // Load sound
    Sound fxWav = LoadSound("resources/sound.wav");
    
    // Load music stream
    Music music = LoadMusicStream("resources/music.ogg");
    
    SetTargetFPS(60);
    
    while (!WindowShouldClose())
    {
        // Update music stream
        UpdateMusicStream(music);
        
        // Check input
        if (IsKeyPressed(KEY_SPACE)) PlaySound(fxWav);
        
        BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawText("Press SPACE to play sound", 190, 200, 20, LIGHTGRAY);
        EndDrawing();
    }
    
    // Clean up
    UnloadSound(fxWav);
    UnloadMusicStream(music);
    CloseAudioDevice();
    CloseWindow();
    
    return 0;
}
```

## Audio Device Management

### Initialization and Configuration
```c
// Initialize audio device
InitAudioDevice();

// Check if audio device is ready
if (IsAudioDeviceReady()) {
    TraceLog(LOG_INFO, "Audio device initialized successfully");
}

// Set master volume (0.0 to 1.0)
SetMasterVolume(0.5f);

// Close audio device when done
CloseAudioDevice();
```

### Audio Context Information
```c
// Get audio device information
void PrintAudioDeviceInfo() {
    if (IsAudioDeviceReady()) {
        // Note: Raylib doesn't expose detailed device info
        // but you can use these checks
        DrawText("Audio Device: Ready", 10, 10, 20, GREEN);
    } else {
        DrawText("Audio Device: Not Ready", 10, 10, 20, RED);
    }
}
```

## Loading and Playing Sounds

### Basic Sound Operations
```c
// Load sound from file
Sound sound = LoadSound("sound.wav");

// Load sound from wave
Wave wave = LoadWave("sound.wav");
Sound soundFromWave = LoadSoundFromWave(wave);
UnloadWave(wave);  // Wave can be unloaded after creating sound

// Play sound
PlaySound(sound);

// Play sound multiple times (multichannel)
PlaySoundMulti(sound);

// Check if sound is playing
if (IsSoundPlaying(sound)) {
    DrawText("Sound is playing", 10, 10, 20, GREEN);
}

// Stop sound
StopSound(sound);

// Pause/Resume
PauseSound(sound);
ResumeSound(sound);

// Set volume (0.0 to 1.0)
SetSoundVolume(sound, 0.5f);

// Set pitch (1.0 is normal)
SetSoundPitch(sound, 1.5f);  // Higher pitch

// Unload sound
UnloadSound(sound);
```

### Multiple Sound Instances
```c
// Sound pool for multiple instances
typedef struct {
    Sound sound;
    bool playing;
    float position;
} SoundInstance;

#define MAX_SOUNDS 10
SoundInstance soundPool[MAX_SOUNDS] = { 0 };

// Initialize sound pool
void InitSoundPool(const char *filename) {
    for (int i = 0; i < MAX_SOUNDS; i++) {
        soundPool[i].sound = LoadSound(filename);
        soundPool[i].playing = false;
    }
}

// Play sound from pool
void PlaySoundFromPool() {
    for (int i = 0; i < MAX_SOUNDS; i++) {
        if (!soundPool[i].playing) {
            PlaySound(soundPool[i].sound);
            soundPool[i].playing = true;
            break;
        }
    }
}

// Update sound pool
void UpdateSoundPool() {
    for (int i = 0; i < MAX_SOUNDS; i++) {
        if (soundPool[i].playing && !IsSoundPlaying(soundPool[i].sound)) {
            soundPool[i].playing = false;
        }
    }
}
```

### Sound Effects Manager
```c
typedef enum {
    SOUND_JUMP,
    SOUND_COIN,
    SOUND_HIT,
    SOUND_EXPLOSION,
    SOUND_COUNT
} SoundEffect;

typedef struct {
    Sound sounds[SOUND_COUNT];
    float masterVolume;
    bool muted;
} SoundManager;

SoundManager soundManager = { 0 };

void InitSoundManager() {
    soundManager.sounds[SOUND_JUMP] = LoadSound("jump.wav");
    soundManager.sounds[SOUND_COIN] = LoadSound("coin.wav");
    soundManager.sounds[SOUND_HIT] = LoadSound("hit.wav");
    soundManager.sounds[SOUND_EXPLOSION] = LoadSound("explosion.wav");
    soundManager.masterVolume = 1.0f;
    soundManager.muted = false;
}

void PlaySoundEffect(SoundEffect effect) {
    if (!soundManager.muted) {
        SetSoundVolume(soundManager.sounds[effect], soundManager.masterVolume);
        PlaySound(soundManager.sounds[effect]);
    }
}

void SetSoundManagerVolume(float volume) {
    soundManager.masterVolume = volume;
    SetMasterVolume(volume);
}

void ToggleMute() {
    soundManager.muted = !soundManager.muted;
    SetMasterVolume(soundManager.muted ? 0.0f : soundManager.masterVolume);
}
```

## Music Streaming

### Basic Music Operations
```c
// Load music stream
Music music = LoadMusicStream("music.ogg");

// Play music
PlayMusicStream(music);

// Update music buffer (call every frame)
UpdateMusicStream(music);

// Control playback
PauseMusicStream(music);
ResumeMusicStream(music);
StopMusicStream(music);

// Check if playing
if (IsMusicStreamPlaying(music)) {
    DrawText("Music playing", 10, 10, 20, GREEN);
}

// Set volume
SetMusicVolume(music, 0.7f);

// Set pitch
SetMusicPitch(music, 1.0f);

// Get time information
float timeLength = GetMusicTimeLength(music);
float timePlayed = GetMusicTimePlayed(music);

// Seek to position (in seconds)
SeekMusicStream(music, 30.0f);  // Jump to 30 seconds

// Unload music
UnloadMusicStream(music);
```

### Music Player Implementation
```c
typedef struct {
    Music music;
    bool isPlaying;
    bool isPaused;
    float volume;
    char *filename;
} MusicPlayer;

MusicPlayer player = { 0 };

void LoadMusicFile(const char *filename) {
    if (player.music.stream.buffer != NULL) {
        UnloadMusicStream(player.music);
    }
    
    player.music = LoadMusicStream(filename);
    player.filename = (char *)filename;
    player.volume = 1.0f;
    player.isPlaying = false;
    player.isPaused = false;
}

void UpdateMusicPlayer() {
    if (player.isPlaying && !player.isPaused) {
        UpdateMusicStream(player.music);
        
        // Loop music
        float timePlayed = GetMusicTimePlayed(player.music);
        float timeLength = GetMusicTimeLength(player.music);
        
        if (timePlayed >= timeLength) {
            SeekMusicStream(player.music, 0.0f);
        }
    }
}

void PlayMusic() {
    if (!player.isPlaying) {
        PlayMusicStream(player.music);
        player.isPlaying = true;
        player.isPaused = false;
    } else if (player.isPaused) {
        ResumeMusicStream(player.music);
        player.isPaused = false;
    }
}

void PauseMusic() {
    if (player.isPlaying && !player.isPaused) {
        PauseMusicStream(player.music);
        player.isPaused = true;
    }
}

void StopMusic() {
    if (player.isPlaying) {
        StopMusicStream(player.music);
        player.isPlaying = false;
        player.isPaused = false;
    }
}

void DrawMusicPlayer(int x, int y) {
    // Draw progress bar
    float progress = GetMusicTimePlayed(player.music) / GetMusicTimeLength(player.music);
    DrawRectangle(x, y, 300, 20, LIGHTGRAY);
    DrawRectangle(x, y, (int)(300 * progress), 20, MAROON);
    
    // Draw time
    DrawText(TextFormat("%.1f / %.1f", 
        GetMusicTimePlayed(player.music), 
        GetMusicTimeLength(player.music)), 
        x, y + 25, 20, BLACK);
    
    // Draw controls
    if (GuiButton((Rectangle){x, y + 50, 60, 30}, 
        player.isPlaying && !player.isPaused ? "Pause" : "Play")) {
        if (player.isPlaying && !player.isPaused) PauseMusic();
        else PlayMusic();
    }
    
    if (GuiButton((Rectangle){x + 70, y + 50, 60, 30}, "Stop")) {
        StopMusic();
    }
}
```

### Playlist System
```c
typedef struct {
    char **files;
    int count;
    int current;
    Music currentMusic;
    bool shuffle;
    bool repeat;
} Playlist;

Playlist playlist = { 0 };

void InitPlaylist(char **files, int count) {
    playlist.files = files;
    playlist.count = count;
    playlist.current = 0;
    playlist.shuffle = false;
    playlist.repeat = true;
}

void PlayNextTrack() {
    if (playlist.currentMusic.stream.buffer != NULL) {
        UnloadMusicStream(playlist.currentMusic);
    }
    
    if (playlist.shuffle) {
        playlist.current = GetRandomValue(0, playlist.count - 1);
    } else {
        playlist.current = (playlist.current + 1) % playlist.count;
    }
    
    playlist.currentMusic = LoadMusicStream(playlist.files[playlist.current]);
    PlayMusicStream(playlist.currentMusic);
}

void PlayPreviousTrack() {
    if (playlist.currentMusic.stream.buffer != NULL) {
        UnloadMusicStream(playlist.currentMusic);
    }
    
    playlist.current = (playlist.current - 1 + playlist.count) % playlist.count;
    playlist.currentMusic = LoadMusicStream(playlist.files[playlist.current]);
    PlayMusicStream(playlist.currentMusic);
}

void UpdatePlaylist() {
    if (playlist.currentMusic.stream.buffer != NULL) {
        UpdateMusicStream(playlist.currentMusic);
        
        // Check if track finished
        if (GetMusicTimePlayed(playlist.currentMusic) >= 
            GetMusicTimeLength(playlist.currentMusic)) {
            if (playlist.repeat || playlist.current < playlist.count - 1) {
                PlayNextTrack();
            }
        }
    }
}
```

## Audio Streams

### Raw Audio Stream
```c
// Create audio stream
AudioStream stream = LoadAudioStream(44100, 16, 2);  // 44.1kHz, 16-bit, stereo

// Play audio stream
PlayAudioStream(stream);

// Generate and play sine wave
void UpdateSineWave(AudioStream stream, float frequency) {
    #define SAMPLE_RATE 44100
    #define STREAM_BUFFER_SIZE 1024
    
    static float phase = 0.0f;
    short samples[STREAM_BUFFER_SIZE];
    
    // Only update if stream needs data
    if (IsAudioStreamProcessed(stream)) {
        for (int i = 0; i < STREAM_BUFFER_SIZE/2; i++) {
            float sample = sinf(2.0f * PI * frequency * phase / SAMPLE_RATE) * 0.5f;
            
            // Convert to 16-bit integer
            short value = (short)(sample * 32767);
            
            // Stereo: left and right channels
            samples[i*2] = value;
            samples[i*2 + 1] = value;
            
            phase += 1.0f;
            if (phase >= SAMPLE_RATE) phase -= SAMPLE_RATE;
        }
        
        UpdateAudioStream(stream, samples, STREAM_BUFFER_SIZE);
    }
}

// Clean up
StopAudioStream(stream);
UnloadAudioStream(stream);
```

### Audio Synthesizer
```c
typedef enum {
    WAVE_SINE,
    WAVE_SQUARE,
    WAVE_TRIANGLE,
    WAVE_SAWTOOTH,
    WAVE_NOISE
} WaveType;

typedef struct {
    AudioStream stream;
    WaveType waveType;
    float frequency;
    float amplitude;
    float phase;
    bool playing;
} Synthesizer;

float GenerateWaveSample(WaveType type, float phase, float frequency) {
    switch (type) {
        case WAVE_SINE:
            return sinf(2.0f * PI * phase);
            
        case WAVE_SQUARE:
            return sinf(2.0f * PI * phase) > 0 ? 1.0f : -1.0f;
            
        case WAVE_TRIANGLE: {
            float p = fmodf(phase, 1.0f);
            return p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p;
        }
        
        case WAVE_SAWTOOTH:
            return 2.0f * fmodf(phase, 1.0f) - 1.0f;
            
        case WAVE_NOISE:
            return GetRandomValue(-100, 100) / 100.0f;
            
        default:
            return 0.0f;
    }
}

void UpdateSynthesizer(Synthesizer *synth) {
    if (!synth->playing) return;
    
    #define BUFFER_SIZE 512
    short buffer[BUFFER_SIZE];
    
    if (IsAudioStreamProcessed(synth->stream)) {
        for (int i = 0; i < BUFFER_SIZE/2; i++) {
            float sample = GenerateWaveSample(synth->waveType, 
                synth->phase, synth->frequency) * synth->amplitude;
            
            short value = (short)(sample * 32767);
            buffer[i*2] = value;      // Left channel
            buffer[i*2 + 1] = value;  // Right channel
            
            synth->phase += synth->frequency / 44100.0f;
            if (synth->phase > 1.0f) synth->phase -= 1.0f;
        }
        
        UpdateAudioStream(synth->stream, buffer, BUFFER_SIZE);
    }
}
```

### Audio Recording
```c
// Note: Raylib doesn't have built-in recording, but here's a concept
typedef struct {
    Wave wave;
    float *samples;
    int sampleCount;
    int maxSamples;
    bool recording;
} AudioRecorder;

void InitAudioRecorder(AudioRecorder *recorder, float seconds) {
    recorder->maxSamples = (int)(44100 * seconds);
    recorder->samples = (float *)MemAlloc(sizeof(float) * recorder->maxSamples);
    recorder->sampleCount = 0;
    recorder->recording = false;
}

void StartRecording(AudioRecorder *recorder) {
    recorder->recording = true;
    recorder->sampleCount = 0;
}

void StopRecording(AudioRecorder *recorder) {
    recorder->recording = false;
    
    // Create wave from recorded samples
    recorder->wave.frameCount = recorder->sampleCount;
    recorder->wave.sampleRate = 44100;
    recorder->wave.sampleSize = 32;  // 32-bit float
    recorder->wave.channels = 1;
    recorder->wave.data = recorder->samples;
}

Sound GetRecordedSound(AudioRecorder *recorder) {
    return LoadSoundFromWave(recorder->wave);
}
```

## 3D Audio

### Spatial Audio (Conceptual)
```c
// Simple 3D audio system
typedef struct {
    Sound sound;
    Vector3 position;
    float radius;
    float volume;
} Sound3D;

void PlaySound3D(Sound3D *sound3d, Vector3 listenerPos) {
    float distance = Vector3Distance(sound3d->position, listenerPos);
    
    if (distance < sound3d->radius) {
        // Calculate volume based on distance
        float attenuation = 1.0f - (distance / sound3d->radius);
        SetSoundVolume(sound3d->sound, sound3d->volume * attenuation);
        
        // Simple stereo panning
        Vector3 direction = Vector3Normalize(
            Vector3Subtract(sound3d->position, listenerPos));
        float pan = direction.x;  // -1 (left) to 1 (right)
        
        // Note: Raylib doesn't have built-in panning, 
        // but you could implement it with audio streams
        
        PlaySound(sound3d->sound);
    }
}

// Doppler effect simulation
void UpdateSound3DPitch(Sound3D *sound3d, Vector3 listenerPos, 
                       Vector3 listenerVel, Vector3 soundVel) {
    Vector3 relativeVel = Vector3Subtract(listenerVel, soundVel);
    Vector3 direction = Vector3Normalize(
        Vector3Subtract(sound3d->position, listenerPos));
    
    float dopplerFactor = Vector3DotProduct(relativeVel, direction);
    float speedOfSound = 343.0f;  // m/s
    
    float pitch = 1.0f / (1.0f - dopplerFactor / speedOfSound);
    SetSoundPitch(sound3d->sound, pitch);
}
```

## Audio Effects

### Echo/Delay Effect
```c
typedef struct {
    float *buffer;
    int bufferSize;
    int writePos;
    float feedback;
    float mix;
} DelayEffect;

DelayEffect* CreateDelay(float delaySeconds, float feedback, float mix) {
    DelayEffect *delay = (DelayEffect *)MemAlloc(sizeof(DelayEffect));
    delay->bufferSize = (int)(44100 * delaySeconds);
    delay->buffer = (float *)MemAlloc(sizeof(float) * delay->bufferSize);
    delay->writePos = 0;
    delay->feedback = feedback;
    delay->mix = mix;
    
    // Clear buffer
    for (int i = 0; i < delay->bufferSize; i++) {
        delay->buffer[i] = 0.0f;
    }
    
    return delay;
}

float ProcessDelay(DelayEffect *delay, float input) {
    float delayed = delay->buffer[delay->writePos];
    delay->buffer[delay->writePos] = input + delayed * delay->feedback;
    
    delay->writePos = (delay->writePos + 1) % delay->bufferSize;
    
    return input + delayed * delay->mix;
}
```

### Simple Reverb
```c
typedef struct {
    DelayEffect *delays[4];
    float roomSize;
    float damping;
} ReverbEffect;

ReverbEffect* CreateReverb(float roomSize, float damping) {
    ReverbEffect *reverb = (ReverbEffect *)MemAlloc(sizeof(ReverbEffect));
    reverb->roomSize = roomSize;
    reverb->damping = damping;
    
    // Create multiple delay lines with different times
    float delayTimes[] = { 0.0297f, 0.0371f, 0.0411f, 0.0437f };
    
    for (int i = 0; i < 4; i++) {
        reverb->delays[i] = CreateDelay(
            delayTimes[i] * roomSize, 
            0.5f, 
            0.25f
        );
    }
    
    return reverb;
}

float ProcessReverb(ReverbEffect *reverb, float input) {
    float output = 0.0f;
    
    for (int i = 0; i < 4; i++) {
        output += ProcessDelay(reverb->delays[i], input);
    }
    
    return input + output * 0.25f;  // Mix with dry signal
}
```

### Low-Pass Filter
```c
typedef struct {
    float cutoff;
    float resonance;
    float a0, a1, a2, b1, b2;
    float z1, z2;
} LowPassFilter;

void UpdateLowPassCoefficients(LowPassFilter *filter, float sampleRate) {
    float omega = 2.0f * PI * filter->cutoff / sampleRate;
    float sin_omega = sinf(omega);
    float cos_omega = cosf(omega);
    float alpha = sin_omega / (2.0f * filter->resonance);
    
    float b0 = (1.0f - cos_omega) / 2.0f;
    float b1 = 1.0f - cos_omega;
    float b2 = (1.0f - cos_omega) / 2.0f;
    float a0 = 1.0f + alpha;
    float a1 = -2.0f * cos_omega;
    float a2 = 1.0f - alpha;
    
    // Normalize
    filter->a0 = b0 / a0;
    filter->a1 = b1 / a0;
    filter->a2 = b2 / a0;
    filter->b1 = a1 / a0;
    filter->b2 = a2 / a0;
}

float ProcessLowPass(LowPassFilter *filter, float input) {
    float output = filter->a0 * input + filter->a1 * filter->z1 + 
                  filter->a2 * filter->z2 - filter->b1 * filter->z1 - 
                  filter->b2 * filter->z2;
    
    filter->z2 = filter->z1;
    filter->z1 = input;
    
    return output;
}
```

## Audio Processing

### Wave Manipulation
```c
// Load and process wave
Wave wave = LoadWave("sound.wav");

// Convert format
WaveFormat(&wave, 44100, 16, 2);  // 44.1kHz, 16-bit, stereo

// Copy wave
Wave waveCopy = WaveCopy(wave);

// Crop wave (in samples)
WaveCrop(&wave, 1000, 5000);  // Keep samples 1000-5000

// Get samples as float array
float *samples = LoadWaveSamples(wave);
int sampleCount = wave.frameCount * wave.channels;

// Process samples (example: normalize)
float maxSample = 0.0f;
for (int i = 0; i < sampleCount; i++) {
    if (fabsf(samples[i]) > maxSample) {
        maxSample = fabsf(samples[i]);
    }
}

if (maxSample > 0.0f) {
    for (int i = 0; i < sampleCount; i++) {
        samples[i] /= maxSample;
    }
}

// Update wave data
// Note: This is conceptual - Raylib doesn't directly support this
// You would need to recreate the wave with modified samples

UnloadWaveSamples(samples);
UnloadWave(wave);
```

### Audio Analysis
```c
// Simple peak detection
typedef struct {
    float *samples;
    int sampleCount;
    float threshold;
    int *peaks;
    int peakCount;
} PeakDetector;

void DetectPeaks(PeakDetector *detector) {
    detector->peakCount = 0;
    
    for (int i = 1; i < detector->sampleCount - 1; i++) {
        float current = fabsf(detector->samples[i]);
        float prev = fabsf(detector->samples[i-1]);
        float next = fabsf(detector->samples[i+1]);
        
        if (current > detector->threshold && 
            current > prev && current > next) {
            detector->peaks[detector->peakCount++] = i;
        }
    }
}

// Simple FFT visualization (conceptual)
void DrawSpectrum(float *samples, int sampleCount, int x, int y, 
                 int width, int height) {
    // This is a simplified visualization
    // Real FFT would require a proper FFT library
    
    int barCount = 32;
    float barWidth = (float)width / barCount;
    
    for (int i = 0; i < barCount; i++) {
        float sum = 0.0f;
        int samplesPerBar = sampleCount / barCount;
        
        for (int j = 0; j < samplesPerBar; j++) {
            int idx = i * samplesPerBar + j;
            if (idx < sampleCount) {
                sum += fabsf(samples[idx]);
            }
        }
        
        float average = sum / samplesPerBar;
        int barHeight = (int)(average * height);
        
        DrawRectangle(x + i * barWidth, y + height - barHeight, 
                     barWidth - 2, barHeight, RED);
    }
}
```

## Real-time Audio

### Audio Callback System
```c
// Audio processor with effects chain
typedef struct {
    AudioStream stream;
    float *inputBuffer;
    float *outputBuffer;
    int bufferSize;
    
    // Effects
    LowPassFilter *lowPass;
    DelayEffect *delay;
    float volume;
} AudioProcessor;

void ProcessAudioBuffer(AudioProcessor *processor) {
    if (IsAudioStreamProcessed(processor->stream)) {
        // Process each sample
        for (int i = 0; i < processor->bufferSize; i++) {
            float sample = processor->inputBuffer[i];
            
            // Apply effects chain
            sample = ProcessLowPass(processor->lowPass, sample);
            sample = ProcessDelay(processor->delay, sample);
            sample *= processor->volume;
            
            // Clipping prevention
            if (sample > 1.0f) sample = 1.0f;
            if (sample < -1.0f) sample = -1.0f;
            
            processor->outputBuffer[i] = sample;
        }
        
        // Convert float to 16-bit for output
        short outputSamples[1024];
        for (int i = 0; i < processor->bufferSize; i++) {
            outputSamples[i] = (short)(processor->outputBuffer[i] * 32767);
        }
        
        UpdateAudioStream(processor->stream, outputSamples, processor->bufferSize);
    }
}
```

### Audio Mixer
```c
typedef struct {
    Sound *sounds;
    float *volumes;
    float *panning;  // -1.0 (left) to 1.0 (right)
    bool *playing;
    int channelCount;
    float masterVolume;
} AudioMixer;

AudioMixer* CreateMixer(int channels) {
    AudioMixer *mixer = (AudioMixer *)MemAlloc(sizeof(AudioMixer));
    mixer->channelCount = channels;
    mixer->sounds = (Sound *)MemAlloc(sizeof(Sound) * channels);
    mixer->volumes = (float *)MemAlloc(sizeof(float) * channels);
    mixer->panning = (float *)MemAlloc(sizeof(float) * channels);
    mixer->playing = (bool *)MemAlloc(sizeof(bool) * channels);
    mixer->masterVolume = 1.0f;
    
    for (int i = 0; i < channels; i++) {
        mixer->volumes[i] = 1.0f;
        mixer->panning[i] = 0.0f;
        mixer->playing[i] = false;
    }
    
    return mixer;
}

void SetChannelVolume(AudioMixer *mixer, int channel, float volume) {
    if (channel >= 0 && channel < mixer->channelCount) {
        mixer->volumes[channel] = volume;
        SetSoundVolume(mixer->sounds[channel], 
                      volume * mixer->masterVolume);
    }
}

void PlayOnChannel(AudioMixer *mixer, int channel, Sound sound) {
    if (channel >= 0 && channel < mixer->channelCount) {
        if (mixer->playing[channel]) {
            StopSound(mixer->sounds[channel]);
        }
        
        mixer->sounds[channel] = sound;
        mixer->playing[channel] = true;
        SetSoundVolume(sound, mixer->volumes[channel] * mixer->masterVolume);
        PlaySound(sound);
    }
}

void UpdateMixer(AudioMixer *mixer) {
    for (int i = 0; i < mixer->channelCount; i++) {
        if (mixer->playing[i] && !IsSoundPlaying(mixer->sounds[i])) {
            mixer->playing[i] = false;
        }
    }
}
```

## Best Practices

### Memory Management
```c
// Resource pooling
typedef struct {
    Sound *sounds;
    char **filenames;
    int count;
    int capacity;
} SoundCache;

Sound GetCachedSound(SoundCache *cache, const char *filename) {
    // Check if already loaded
    for (int i = 0; i < cache->count; i++) {
        if (strcmp(cache->filenames[i], filename) == 0) {
            return cache->sounds[i];
        }
    }
    
    // Load new sound
    if (cache->count < cache->capacity) {
        cache->sounds[cache->count] = LoadSound(filename);
        cache->filenames[cache->count] = (char *)MemAlloc(strlen(filename) + 1);
        strcpy(cache->filenames[cache->count], filename);
        cache->count++;
        return cache->sounds[cache->count - 1];
    }
    
    // Cache full
    TraceLog(LOG_WARNING, "Sound cache full!");
    return (Sound){0};
}

void UnloadSoundCache(SoundCache *cache) {
    for (int i = 0; i < cache->count; i++) {
        UnloadSound(cache->sounds[i]);
        MemFree(cache->filenames[i]);
    }
    MemFree(cache->sounds);
    MemFree(cache->filenames);
}
```

### Performance Tips

1. **Use Appropriate Formats**
   - OGG for music (compressed)
   - WAV for short sound effects
   - Stream large files, load small ones

2. **Manage Resources**
   ```c
   // Load sounds at startup
   void LoadGameSounds() {
       // Load all sounds once
   }
   
   // Unload when done
   void UnloadGameSounds() {
       // Unload all sounds
   }
   ```

3. **Audio Settings**
   ```c
   typedef struct {
       float masterVolume;
       float musicVolume;
       float sfxVolume;
       bool muteMusic;
       bool muteSFX;
   } AudioSettings;
   
   void ApplyAudioSettings(AudioSettings *settings) {
       SetMasterVolume(settings->masterVolume);
       // Apply other settings
   }
   
   void SaveAudioSettings(AudioSettings *settings) {
       SaveFileData("audio_settings.dat", settings, sizeof(AudioSettings));
   }
   
   void LoadAudioSettings(AudioSettings *settings) {
       unsigned int dataSize = 0;
       unsigned char *data = LoadFileData("audio_settings.dat", &dataSize);
       if (data != NULL && dataSize == sizeof(AudioSettings)) {
           memcpy(settings, data, sizeof(AudioSettings));
           UnloadFileData(data);
       }
   }
   ```

4. **Error Handling**
   ```c
   Sound SafeLoadSound(const char *filename) {
       if (!FileExists(filename)) {
           TraceLog(LOG_ERROR, "Sound file not found: %s", filename);
           return (Sound){0};
       }
       
       Sound sound = LoadSound(filename);
       if (sound.frameCount == 0) {
           TraceLog(LOG_ERROR, "Failed to load sound: %s", filename);
       }
       
       return sound;
   }
   ```

5. **Audio Debugging**
   ```c
   void DrawAudioDebug(int x, int y) {
       DrawText(TextFormat("Sounds Playing: %d", GetSoundsPlaying()), 
               x, y, 20, BLACK);
       
       // Draw volume meters, waveforms, etc.
   }
   ```

Remember: Always initialize audio device before using audio functions and clean up resources when done!