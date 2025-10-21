#ifndef CAMERA_CLIENT_H
#define CAMERA_CLIENT_H

void camera_client_start(void);
void camera_client_fetch_image(int camera_index);  // Updated to take camera index (1-8)

#endif // CAMERA_CLIENT_H