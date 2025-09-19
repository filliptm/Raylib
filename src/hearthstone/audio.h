#ifndef AUDIO_H
#define AUDIO_H

#include "errors.h"
#include "config.h"
#include "raylib.h"

typedef enum {
    SOUND_CARD_PLAY,
    SOUND_CARD_DRAW,
    SOUND_CARD_ATTACK,
    SOUND_CARD_DEATH,
    SOUND_SPELL_CAST,
    SOUND_DAMAGE,
    SOUND_HEAL,
    SOUND_TURN_START,
    SOUND_TURN_END,
    SOUND_VICTORY,
    SOUND_DEFEAT,
    SOUND_UI_CLICK,
    SOUND_UI_HOVER,
    SOUND_MAX
} GameSoundType;

typedef enum {
    MUSIC_MENU,
    MUSIC_GAMEPLAY,
    MUSIC_COMBAT,
    MUSIC_VICTORY,
    MUSIC_DEFEAT,
    MUSIC_MAX
} GameMusicType;

typedef struct AudioSystem {
    Sound sounds[SOUND_MAX];
    Music music[MUSIC_MAX];
    
    GameMusicType currentMusic;
    bool musicEnabled;
    bool soundEnabled;
    
    float masterVolume;
    float musicVolume;
    float soundVolume;
    
    // Sound variations for randomization
    Sound* cardPlaySounds;
    int cardPlaySoundCount;
    
    Sound* attackSounds;
    int attackSoundCount;
} AudioSystem;

// Audio system management
GameError InitAudioSystem(AudioSystem* audio, const GameConfig* config);
void CleanupAudioSystem(AudioSystem* audio);

// Sound playback
GameError PlayGameSound(AudioSystem* audio, GameSoundType sound);
GameError PlayRandomSound(AudioSystem* audio, Sound* sounds, int count);
GameError PlaySoundAtPosition(AudioSystem* audio, GameSoundType sound, Vector3 position);

// Music playback
GameError PlayGameMusic(AudioSystem* audio, GameMusicType music);
GameError StopGameMusic(AudioSystem* audio);
GameError PauseGameMusic(AudioSystem* audio);
GameError ResumeGameMusic(AudioSystem* audio);
GameError FadeMusic(AudioSystem* audio, float targetVolume, float duration);

// Volume control
GameError SetGameMasterVolume(AudioSystem* audio, float volume);
GameError SetGameMusicVolume(AudioSystem* audio, float volume);
GameError SetGameSoundVolume(AudioSystem* audio, float volume);
GameError UpdateVolumesFromConfig(AudioSystem* audio, const GameConfig* config);

// Audio effects
void PlayCardPlaySound(AudioSystem* audio);
void PlayCardAttackSound(AudioSystem* audio);
void PlayDamageSound(AudioSystem* audio, int damage);
void PlayHealSound(AudioSystem* audio);
void PlayVictorySound(AudioSystem* audio);
void PlayDefeatSound(AudioSystem* audio);

// 3D Audio
void Update3DAudioListener(Camera3D camera);

#endif