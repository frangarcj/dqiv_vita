/* NDK AAssetManager subset consumed by libdq4core (LoadFileList) and libsdlib (BGM streaming). */
#ifndef DQIVN_ASSET_MANAGER_H
#define DQIVN_ASSET_MANAGER_H
#include <stdint.h>
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct AAssetManager AAssetManager;
typedef struct AAsset AAsset;
AAssetManager *AAssetManager_fromJava(void *env, void *assetManager);
AAsset *AAssetManager_open(AAssetManager *mgr, const char *filename, int mode);
int AAsset_read(AAsset *asset, void *buf, size_t count);
off_t AAsset_seek(AAsset *asset, off_t offset, int whence);
off_t AAsset_getLength(AAsset *asset);
off_t AAsset_getRemainingLength(AAsset *asset);
int64_t AAsset_seek64(AAsset *asset, int64_t offset, int whence);
int64_t AAsset_getLength64(AAsset *asset);
int64_t AAsset_getRemainingLength64(AAsset *asset);
void AAsset_close(AAsset *asset);
#ifdef __cplusplus
}
#endif
#endif
