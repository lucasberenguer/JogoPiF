#ifndef MENU_VIDEO_H
#define MENU_VIDEO_H

typedef struct MenuVideo MenuVideo;

MenuVideo *OpenMenuVideo(const char *path, int *width, int *height, double *frameInterval);
const unsigned char *ReadMenuVideo(MenuVideo *video);
void CloseMenuVideo(MenuVideo *video);

#endif
