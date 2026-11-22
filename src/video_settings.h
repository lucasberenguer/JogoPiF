#ifndef VIDEO_SETTINGS_H
#define VIDEO_SETTINGS_H
#include <stdbool.h>

/* Opções de vídeo escolhidas nas Configurações e salvas em video_pif.cfg,
   ao lado do executável. */
enum { VIDEO_RENDER_SCALES = 3, VIDEO_FPS_OPTIONS = 2 };
#define VIDEO_WINDOW_WIDTH 1280  /* tamanho da janela fora da tela cheia */
#define VIDEO_WINDOW_HEIGHT 720

typedef struct VideoSettings {
    bool fullscreen;   /* tela cheia sem borda */
    int renderScale;   /* porcentagem da resolução de renderização: 50, 75 ou 100 */
    int fpsLimit;      /* 30 ou 60 */
    bool showFps;      /* contador de FPS no canto da tela */
} VideoSettings;

VideoSettings *GetVideoSettings(void);
void LoadVideoSettings(VideoSettings *video);
void SaveVideoSettings(const VideoSettings *video);

/* Fora da tela cheia: janela de 1280x720 (limitada ao monitor), centralizada. */
void ApplyVideoWindowSize(void);

void CycleVideoRenderScale(VideoSettings *video, int direction);
void CycleVideoFpsLimit(VideoSettings *video, int direction);

/* Substituem BeginDrawing/EndDrawing. Com renderização abaixo de 100%, o quadro é
   desenhado numa imagem menor e ampliado para a tela; a 100% o caminho é o mesmo de antes.
   EndGameFrame também desenha o contador de FPS, quando ligado. */
void BeginGameFrame(void);
void EndGameFrame(void);
/* BeginScissorMode em coordenadas de tela, correto também com renderização reduzida. */
void BeginGameScissor(int x, int y, int width, int height);
void UnloadVideoFrame(void);
#endif
