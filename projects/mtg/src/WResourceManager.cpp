#include "../include/config.h"
#include "../include/utils.h"
#include "../include/GameOptions.h"
#include <iostream>
#include <fstream>
#include <string>
#include <stdlib.h>
#include <JGE.h>
#include <JFileSystem.h>
#include "../include/WResourceManager.h"


WResourceManager resources;

WCachedResource::WCachedResource(){
  locks = 0;
  lastTime = resources.nowTime();
}

bool WCachedResource::isLocked(){
  return (locks != 0);
}

void WCachedResource::lock(){
  if(locks < 255)
    locks++;
}

void WCachedResource::unlock(bool force){
  if(force)
    locks = 0;
  else if(locks > 0)
    locks--;
}

void WCachedResource::hit(){
  lastTime = resources.nowTime();
}

WCachedTexture::WCachedTexture(){
  texture = NULL;
  bVRAM = false;
}

WCachedTexture::~WCachedTexture(){
  SAFE_DELETE(texture);
  for(vector<JQuad*>::iterator i = trackedQuads.begin();i!=trackedQuads.end();i++)
    SAFE_DELETE((*i));
}

JTexture * WCachedTexture::GetTexture(){
  return texture;
}

bool WCachedTexture::ReleaseQuad(JQuad* quad){
 for(vector<JQuad*>::iterator i = trackedQuads.begin();i!=trackedQuads.end();i++){
   if((*i) == quad){
     SAFE_DELETE(quad);
     trackedQuads.erase(i);
     return true;
   }
 }
 return false;
}

JQuad * WCachedTexture::GetQuad(float offX, float offY, float width, float height){
  if(texture == NULL)
    return NULL;

 if(width == 0.0f)
      width = texture->mWidth;
 if(height == 0.0f)
      height = texture->mHeight;

  vector<JQuad*>::iterator it;
  for(it = trackedQuads.begin();it!=trackedQuads.end();it++)
    if((*it)->mHeight == height && (*it)->mWidth == width
      && (*it)->mX == offX && (*it)->mY == offY)
      return (*it);

  JQuad * jq = NEW JQuad(texture,offX,offY,width,height);
  trackedQuads.push_back(jq);
  return jq;
}

JQuad * WCachedTexture::GetCard(float offX, float offY, float width, float height){
 if(texture == NULL)
    return NULL;

 if(width == 0.0f)
      width = texture->mWidth;
 if(height == 0.0f)
      height = texture->mHeight;

  vector<JQuad*>::iterator it;
  for(it = trackedQuads.begin();it!=trackedQuads.end();it++)
    if((*it)->mHeight == height && (*it)->mWidth == width
      && (*it)->mX == offX && (*it)->mY == offY)
      return (*it);

  JQuad * jq = NEW JQuad(texture,offX,offY,width,height);
  jq->SetHotSpot(jq->mTex->mWidth / 2, jq->mTex->mHeight / 2);
  trackedQuads.push_back(jq);
  return jq;
}

WCachedSample::WCachedSample(){
  sample = NULL;
}

WCachedSample::~WCachedSample(){
  SAFE_DELETE(sample);
}

JSample * WCachedSample::GetSample(){
  return sample;
}
void WResourceManager::ClearMisses(){
  map<string,WCachedTexture*>::iterator itTex, nextTex;
  map<string,WCachedSample*>::iterator itSfx, nextSfx;

  for(itTex = textureCache.begin(); itTex != textureCache.end();itTex=nextTex){
    nextTex = itTex;
    nextTex++;

    if(itTex->second == NULL){
      textureCache.erase(itTex);
    }
  }
  for(itSfx = sampleCache.begin(); itSfx != sampleCache.end();itSfx=nextSfx){
    nextSfx = itSfx;
    nextSfx++;

    if(itSfx->second == NULL){
      sampleCache.erase(itSfx);
    }
  }
}

void WResourceManager::ClearUnlocked(){
  map<string,WCachedTexture*>::iterator itTex, nextTex;
  map<string,WCachedSample*>::iterator itSfx, nextSfx;

  for(itTex = textureCache.begin(); itTex != textureCache.end();itTex=nextTex){
    nextTex = itTex;
    nextTex++;

    if(!itTex->second || (itTex->second && !itTex->second->isLocked())){
      textureCache.erase(itTex);
    }
  }
  for(itSfx = sampleCache.begin(); itSfx != sampleCache.end();itSfx=nextSfx){
    nextSfx = itSfx;
    nextSfx++;

    if(!itSfx->second || (itSfx->second && !itSfx->second->isLocked())){
      sampleCache.erase(itSfx);
    }
  }
}

bool WResourceManager::cleanup(){
  long maxSize = options[Options::CACHESIZE].number * 100000;
  if (!maxSize) maxSize = CACHE_SIZE_PIXELS;

  while (textureCache.size() > MAX_CACHE_OBJECTS - 1 || totalsize > maxSize){
    ClearMisses();
    int result = RemoveOldestTexture();
    if (!result) return false;
  }
  return true;
}


unsigned int WResourceManager::nowTime(){
  if(lastTime == 65535)
    FlattenTimes();
  
  return ++lastTime;
}

void WResourceManager::ClearSamples(){
  map<string,WCachedSample*>::iterator next;
  for(map<string,WCachedSample*>::iterator it = sampleCache.begin();it!=sampleCache.end();it=next){
    next = it;
    next++;
    
    if(it->second && !it->second->isLocked())
      SAFE_DELETE(it->second);
  }
  sampleCache.clear();
}

WCachedSample * WResourceManager::getCachedSample(string filename, bool makenew){
  map<string,WCachedSample*>::iterator miss = sampleCache.find(filename);
  if(miss != sampleCache.end() && miss->second == NULL)
    return NULL; //We've found a cache miss, so return null.

  WCachedSample * csample = sampleCache[filename];

  //Failed to cache it!
  if(!csample && makenew){
    //Space in cache, make new sample
    if(cleanup()){
      csample = NEW WCachedSample();
      string sfile = sfxFile(filename);
      if(sfile != ""){
        csample->sample = JSoundSystem::GetInstance()->LoadSample(sfile.c_str());
        //Potential cache overflow- clean the cache
        if(!csample->sample && fileExists(sfile.c_str())){
          ClearSamples();
          csample->sample = JSoundSystem::GetInstance()->LoadSample(sfile.c_str());
        }
      }
      else
        csample->sample = NULL;
      
      //Failed. Leave the cache miss alone.
      if(!csample->sample){
        SAFE_DELETE(csample);
        return NULL;
      }
      //Success!
      csample->hit();
      sampleCache[filename] = csample;
    }
    else
      return NULL; //Error.
  }

  return csample;
}

WCachedTexture * WResourceManager::getCachedTexture(string filename, bool makenew, int mode, int format){
  WCachedTexture * ctex = textureCache[filename];
  //Failed to cache it!
  if(!ctex && makenew){
    //Space in cache, make new texture
    if(cleanup()){
       ctex = NEW WCachedTexture();
       //Within limits, keep removing items from cache until we can create this
       ctex->texture = attemptTexture(graphicsFile(filename));       

       if(!ctex->texture){
        for(map<string,WCachedTexture*>::iterator it=textureCache.begin();it!=textureCache.end();it++)
          if(it->first == filename){
          textureCache.erase(it);
          break;
        }
         SAFE_DELETE(ctex);
         return NULL;
       }
      totalsize+=ctex->texture->mTexHeight *ctex->texture->mTexWidth;   
      char buf[512];
      sprintf(buf,"Cache size: %ld\n",totalsize);
      OutputDebugString(buf);
      ctex->hit();
      textureCache[filename] = ctex;
    }
    else
      return NULL; //Error.
  }

  return ctex;
}

JTexture * WResourceManager::attemptTexture(string filename, int mode, int format){
  JTexture * result = JRenderer::GetInstance()->LoadTexture(filename.c_str(),mode,format);

  if(result == NULL){
    if(!fileExists(filename.c_str()))
      return NULL;

    for(int attempt=0;attempt<10;attempt++){
      if(!RemoveOldestTexture()) 
        break;
      result = JRenderer::GetInstance()->LoadTexture(filename.c_str(),mode,format);
      if(result)
        break;
    }

    //Still no result, so clear cache entirely, then try again.
    if(!result){
      ClearUnlocked();
      result = JRenderer::GetInstance()->LoadTexture(filename.c_str(),mode,format);
    }
  }

  return result;
}


WCachedTexture * WResourceManager:: getCachedCard(MTGCard * card, int type, bool makenew){
  string filename = card->getImageName();
  if(type == CACHE_THUMB)
    filename = "thumbnails/"+filename;

  map<string,WCachedTexture*>::iterator miss = textureCache.find(filename);
  if(miss != textureCache.end() && miss->second == NULL)
    return NULL; //We've found a cache miss, so return null.
  
  WCachedTexture * ctex = textureCache[filename];
  //Failed to find it in cache!
  if(!ctex && makenew){
    if(cleanup()){
      //Space in cache, make new texture
      ctex = NEW WCachedTexture();
      string cfile = cardFile(filename,card->getSetName());
      if(cfile != ""){
        //Within limits, keep removing items from cache until we can create this
        ctex->texture = attemptTexture(cfile);
      }
      else
        ctex->texture = NULL;

      //Couldn't create texture, so fail. Leave failure in cache, so we don't try again later.
      if(!ctex->texture){
        SAFE_DELETE(ctex);
        return NULL;
      }
      totalsize+=ctex->texture->mTexHeight *ctex->texture->mTexWidth;
      char buf[512];
      sprintf(buf,"Cache size: %ld\n",totalsize);
      OutputDebugString(buf);
      ctex->hit();
      textureCache[filename] = ctex;
    }
    else
      return NULL; //Error.
  }

  return ctex;
}

void WResourceManager::FlattenTimes(){
  unsigned int youngest = 65535;
  unsigned int oldest = 0;

  for (map<string,WCachedTexture*>::iterator it = textureCache.begin(); it != textureCache.end(); ++it){
    if(!it->second) continue;
    if(it->second->lastTime < youngest) youngest = it->second->lastTime;
    if(it->second->lastTime > oldest) oldest = it->second->lastTime;
  }
  for (map<string,WCachedSample*>::iterator it = sampleCache.begin(); it != sampleCache.end(); ++it){
    if(!it->second) continue;
    if(it->second->lastTime < youngest) youngest = it->second->lastTime;
    if(it->second->lastTime > oldest) oldest = it->second->lastTime;
  }

  for (map<string,WCachedSample*>::iterator it = sampleCache.begin(); it != sampleCache.end(); ++it){
    if(!it->second) continue;
    it->second->lastTime -= youngest;
  }
  for (map<string,WCachedTexture*>::iterator it = textureCache.begin(); it != textureCache.end(); ++it){
    if(!it->second) continue;
    it->second->lastTime -= youngest;
  }

  lastTime = oldest;
}

bool WResourceManager::RemoveOldestTexture(){
  map<string,WCachedTexture*>::iterator oldest;
  oldest = textureCache.end();
  for(map<string,WCachedTexture*>::iterator it = textureCache.begin();it!=textureCache.end();it++){
    if(it->second && !it->second->isLocked() 
      && (oldest == textureCache.end() || it->second->lastTime < oldest->second->lastTime))
      oldest = it;
  }

  if(oldest != textureCache.end()){
    if(oldest->second){
      if(!oldest->second->isLocked() ){
        if(oldest->second->texture)
          totalsize-=oldest->second->texture->mTexHeight * oldest->second->texture->mTexWidth;
        SAFE_DELETE(oldest->second);
      }
      else
        return false;
    }
    
    textureCache.erase(oldest);
    return true;
  }

  return false;
}

bool WResourceManager::RemoveOldestSample(){
  map<string,WCachedSample*>::iterator it, saved;
  saved = sampleCache.end();

  for(it = sampleCache.begin();it!=sampleCache.end();it++){
    if(it->second && !it->second->isLocked() 
       && (saved == sampleCache.end() || it->second->lastTime < saved->second->lastTime))
      saved = it;
  }

  if(saved != sampleCache.end()){
    if(saved->second){
      if(!saved->second->isLocked()){
        SAFE_DELETE(saved->second);
      }
      else
        return false;
    }

    sampleCache.erase(saved);
    return true;
  }
  return false;
}
WResourceManager::WResourceManager(){	

  #ifdef WIN32
  char buf [4096];
  sprintf(buf, " Init WResourceManager : %p\n", this);
  OutputDebugString(buf);
  #endif

  mTextureList.clear();
	mTextureList.reserve(16);
	mTextureMap.clear();

	mQuadList.clear();
	mQuadList.reserve(128);
	mQuadMap.clear();

	mFontList.clear();
	mFontList.reserve(4);
	mFontMap.clear();

	mMusicList.clear();
	mMusicList.reserve(4);
	mMusicMap.clear();

	mSampleList.clear();
	mSampleList.reserve(8);
	mSampleMap.clear();

  nb_textures = 0;
  totalsize = 0;
  lastTime = 0;
}
WResourceManager::~WResourceManager(){

  LOG("==Destroying WResourceManager==");
  for (map<string,WCachedTexture*>::iterator it = textureCache.begin(); it != textureCache.end(); ++it){
    delete it->second;
  }
  for (map<string,WCachedSample*>::iterator it = sampleCache.begin(); it != sampleCache.end(); ++it){
    delete it->second;
  }

  textureCache.clear();
  sampleCache.clear();

  RemoveAll();
  LOG("==Successfully Destroyed WResourceManager==");

}

JQuad * WResourceManager::RetrieveCard(MTGCard * card, int type, int style){
  //Cards are never, ever resource managed, so just check cache.
  WCachedTexture * tc;
  if(style == RETRIEVE_EXISTING)
    tc = getCachedCard(card,type,false);
  else
    tc = getCachedCard(card,type,true);
  
  //Perform lock or unlock on entry.
  if(tc != NULL){
    if(style == RETRIEVE_LOCK) tc->lock();
    else if(style == RETRIEVE_UNLOCK) tc->unlock();
  }

  //Texture exists! Get quad.
  if(tc && tc->texture != NULL){
    tc->hit();
    return tc->GetCard();  
  }

  return NULL;
}

JQuad * WResourceManager::RetrieveQuad(string filename, float offX, float offY, float width, float height, string resname, int style){

  for(vector<string>::iterator it=mTextureMissing.begin();it!=mTextureMissing.end();it++){
    if((*it) == filename)
      return NULL;
  }

  if(resname == "") 
    resname = filename;

  //Check our managed resources.
  JQuad * retval = GetQuad(resname);
  if(retval != NULL || style == RETRIEVE_RESOURCE)
    return retval;

  //We have a managed resource named this!
  JTexture * jtex = GetTexture(filename);
  if(jtex){
     //Make this quad, overwriting any similarly resname'd quads.
     CreateQuad(resname,filename,offX,offY,width,height);
     return GetQuad(resname);
  }

  //If we don't have an existing texture, check cache.
  WCachedTexture * tc = NULL;
  if(style != RETRIEVE_MANAGE){
  if(style == RETRIEVE_EXISTING)
    tc = getCachedTexture(filename,false);
  else if(style == RETRIEVE_VRAM)
    tc = getCachedTexture(filename,true,TEX_TYPE_USE_VRAM);
  else
    tc = getCachedTexture(filename);
  }
  
  //Quads never mess with locks. Ever.
  if(style == RETRIEVE_MANAGE){
    //Remove cache hit from cache
    map<string,WCachedTexture*>::iterator it = textureCache.end();
    tc = textureCache[filename];
    for(it = textureCache.begin();it!=textureCache.end();it++){
      if(it->first == filename)
        break;
    }
    
    if(it != textureCache.end()){
      SAFE_DELETE(it->second);
      textureCache.erase(it);
    }
    //Pop texture & quad into resource manager
    CreateQuad(resname,filename,offX,offY,width,height);
    return GetQuad(resname);
  }

  //Texture exists! Get quad.
  if(tc && tc->texture != NULL){
    if(style == RETRIEVE_VRAM)
      tc->bVRAM = true;
    tc->hit();
    return tc->GetQuad(offX,offY,width,height);  
  }

  //Texture doesn't exist, so no quad. Record miss.
  mTextureMissing.push_back(filename);
  return NULL;
}
void WResourceManager::Release(JTexture * tex){
  if(tex == NULL)
    return;
  map<string,WCachedTexture*>::iterator it;
  for(it = textureCache.begin();it!=textureCache.end();it++){
    if(it->second && it->second->texture == tex)
      break;
  }

  if(it != textureCache.end()){
    it->second->unlock();
    if(!it->second->isLocked()){
      if(it->second->texture)
      totalsize-=it->second->texture->mTexHeight * it->second->texture->mTexWidth;
      SAFE_DELETE(it->second);
      textureCache.erase(it);
    }
  }
}

void WResourceManager::Release(JQuad * quad){
  map<string,WCachedTexture*>::iterator it;
    if(quad == NULL)
    return;

  for(it = textureCache.begin();it!=textureCache.end();it++){
    if(it->second && it->second->ReleaseQuad(quad))
      break;
  }

  if(it != textureCache.end()){
    if(!it->second->isLocked()){
      SAFE_DELETE(it->second);
      textureCache.erase(it);
    }
  }
}

void WResourceManager::Release(JSample * sample){
  if(sample == NULL)
    return;

  map<string,WCachedSample*>::iterator it;
  for(it = sampleCache.begin();it!=sampleCache.end();it++){
    if(it->second && it->second->sample == sample)
      break;
  }

  if(it != sampleCache.end()){
    it->second->unlock();
    if(!it->second->isLocked()){
      SAFE_DELETE(it->second);
      sampleCache.erase(it);
    }
  }
}

JTexture * WResourceManager::RetrieveTexture(string filename, int style){
//Check our resources.
  JTexture * retval = GetTexture(filename);
  if(retval != NULL || style == RETRIEVE_RESOURCE)
    return retval;

  //Check cache.
  WCachedTexture * tc = NULL;
  if(style != RETRIEVE_MANAGE){
  if(style == RETRIEVE_EXISTING)
    tc = getCachedTexture(filename,false);
  else if(style == RETRIEVE_VRAM)
    tc = getCachedTexture(filename,true,TEX_TYPE_USE_VRAM);
  else
    tc = getCachedTexture(filename);
  }

  if(style == RETRIEVE_MANAGE){
    for(vector<string>::iterator it = mTextureMissing.begin();it!=mTextureMissing.end();it++){
      if((*it) == filename)
        return NULL;
    }
  }
  
  //Perform lock or unlock on entry.
  if(style != RETRIEVE_MANAGE && tc){
    if(style == RETRIEVE_LOCK || style == RETRIEVE_VRAM) tc->lock();
    else if(style == RETRIEVE_UNLOCK) tc->unlock();
  }
  else if(style == RETRIEVE_MANAGE){
    //Remove cache hit from cache
    tc = textureCache[filename];
    map<string,WCachedTexture*>::iterator it = textureCache.end();
      for(it = textureCache.begin();it!=textureCache.end();it++){
        if(it->first == filename)
          break;
    }

    if(it != textureCache.end()){
      SAFE_DELETE(it->second);
      textureCache.erase(it);
    }
    //Pop texture into resource manager
    int val = CreateTexture(filename);
    if(val == INVALID_ID){
      mTextureMissing.push_back(filename);
      return NULL;//file not found
    }

    return GetTexture(val);
  }

  //Texture exists! Get it.
  if(tc && tc->texture != NULL){
    tc->hit();
    return tc->GetTexture();  
  }

  return retval;
}

JSample * WResourceManager::RetrieveSample(string filename, int style){
//Check our resources.
  JSample * retval = GetSample(filename);
  if(retval != NULL || style == RETRIEVE_RESOURCE)
    return retval;

  //Check cache.
  WCachedSample * tc = NULL;

  if(style != RETRIEVE_MANAGE){
    if(style == RETRIEVE_EXISTING)
      tc = getCachedSample(filename,false);
    else
      tc = getCachedSample(filename);
  }

  //Perform lock or unlock on entry.
  if(style != RETRIEVE_MANAGE && tc){
    if(style == RETRIEVE_LOCK || style == RETRIEVE_VRAM) tc->lock();
    else if(style == RETRIEVE_UNLOCK) tc->unlock();
  }
  else if(style == RETRIEVE_MANAGE){
    //Remove cache hit from cache
    tc = sampleCache[filename];
    map<string,WCachedSample*>::iterator it;
      for(it = sampleCache.begin();it!=sampleCache.end();it++){
        if(it->first == filename)
          break;
      }

    if(it != sampleCache.end()){
      SAFE_DELETE(it->second);
      sampleCache.erase(it);
    }
    //Pop sample into resource manager
    LoadSample(filename);
    return GetSample(filename);
  }

  //Sample exists! Get it.
  if(tc && tc->sample != NULL){
    tc->hit();
    return tc->GetSample();  
  }

  return retval;
}

string WResourceManager::graphicsFile(const string filename, const string specific){
    char buf[512];
   
    //Check the specific location, if any.
    if(specific != ""){
      sprintf(buf,"%s/%s",specific.c_str(),filename.c_str());
      if(fileOK(buf,true))
        return buf;
    }

    //Check the theme folder.
    string theme = options[Options::ACTIVE_THEME].str;
    std::transform(theme.begin(), theme.end(), theme.begin(), ::tolower);

    if(theme != "" && theme != "default"){
      sprintf(buf,"themes/%s/%s",theme.c_str(),filename.c_str());
     if(fileOK(buf,true))
        return buf;
    }

    //Failure. Check mode graphics     
    string mode = options[Options::ACTIVE_MODE].str;
    std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);

    if(mode != "" && mode != "defualt"){
      sprintf(buf,"modes/%s/graphics/%s",mode.c_str(),filename.c_str());
      if(fileOK(buf,true))
        return buf;
    }
       
     //Failure. Check graphics       
     char graphdir[512];
     sprintf(graphdir,"graphics/%s",filename.c_str());
      if(fileOK(graphdir,true))
        return graphdir;
    
     //Failure. Check sets.       
     sprintf(buf,"sets/%s",filename.c_str());
      if(fileOK(buf,true))
        return buf;

     //Complete abject failure. Probably a crash...
     return graphdir;
}

string WResourceManager::cardFile(const string filename, const string setname, const string specific){
    JFileSystem* fs = JFileSystem::GetInstance();
    char buf[512];
    char sets[512];

    fs->DetachZipFile();

    if(setname != "")
      sprintf(sets,"sets/%s",setname.c_str());
    else
      sprintf(sets,"sets");

    //Check the specific location, if any.
    if(specific != ""){
      sprintf(buf,"%s/%s/%s",specific.c_str(),sets,filename.c_str());
      if(fileOK(buf,true))
        return buf;
    }

    //Check the theme folder.
    string theme = options[Options::ACTIVE_THEME].str;
    std::transform(theme.begin(), theme.end(), theme.begin(), ::tolower);

    if(theme != "" && theme != "default"){
       sprintf(buf,"themes/%s/%s/%s",theme.c_str(),sets,filename.c_str());
       if(fileOK(buf,true))
         return buf;
    }

    //Failure. Check mode     
    string mode = options[Options::ACTIVE_MODE].str;
    std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);

    if(mode != "" && mode != "defualt"){
      sprintf(buf,"modes/%s/%s/%s",mode.c_str(),sets,filename.c_str());
    if(fileOK(buf,true))
       return buf;      
    }
       
     //Failure. Check sets       
     char defdir[512];
     sprintf(defdir,"%s/%s",sets,filename.c_str());
     if(fileOK(defdir,true))
       return defdir;      

     //Failure. Assume it's in a zip file?
     if(setname.size()){
        char zipname[100];
        sprintf(zipname, "Res/sets/%s/%s.zip", setname.c_str(),setname.c_str());
        if (fileOK(zipname)){
          fs->AttachZipFile(zipname);
          return filename;
        }
     }
    
     //Complete failure.
     return "";
}

string WResourceManager::musicFile(const string filename, const string specific){
    char buf[512];

    //Check the specific location, if any.
    if(specific != ""){
      sprintf(buf,"%s/%s",specific.c_str(),filename.c_str());
      if(fileOK(buf,true))
        return buf;
    }

    //Check the theme folder.
    string theme = options[Options::ACTIVE_THEME].str;
    std::transform(theme.begin(), theme.end(), theme.begin(), ::tolower);

    if(theme != "" && theme != "default"){
       sprintf(buf,"themes/%s/sound/%s",theme.c_str(),filename.c_str());
       if(fileOK(buf,true))
         return buf;
    }

    //Failure. Check mode     
    string mode = options[Options::ACTIVE_MODE].str;
    std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);

    if(mode != "" && mode != "defualt"){
      sprintf(buf,"modes/%s/sound/%s",mode.c_str(),filename.c_str());
    if(fileOK(buf,true))
       return buf;      
    }
       
     //Failure. Check sound       
     char defdir[512];
     sprintf(defdir,"sound/%s",filename.c_str());
     if(fileOK(defdir,true))
       return defdir;      
    
     //Complete abject failure. Probably a crash...
     return defdir;
}

string WResourceManager::sfxFile(const string filename, const string specific){
    char buf[512];

    //Check the specific location, if any.
    if(specific != ""){
      sprintf(buf,"%s/%s",specific.c_str(),filename.c_str());
      if(fileOK(buf,true))
        return buf;
    }

    //Check the theme folder.
    string theme = options[Options::ACTIVE_THEME].str;
    std::transform(theme.begin(), theme.end(), theme.begin(), ::tolower);

    if(theme != "" && theme != "default"){
       sprintf(buf,"themes/%s/sound/sfx/%s",theme.c_str(),filename.c_str());
       if(fileOK(buf,true))
         return buf;
    }

    //Failure. Check mode     
    string mode = options[Options::ACTIVE_MODE].str;
    std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);
    if(mode != "" && mode != "defualt"){
      sprintf(buf,"modes/%s/sound/sfx/%s",mode.c_str(),filename.c_str());
    if(fileOK(buf,true))
       return buf;      
    }
       
     //Failure. Check sound       
     char defdir[512];
     sprintf(defdir,"sound/sfx/%s",filename.c_str());
     if(fileOK(defdir,true))
       return defdir;      
    
     //Complete abject failure. Probably a crash...
     return "";
}

int WResourceManager::fileOK(string filename, bool relative){
  char fname[512];
  std::ifstream * fp = NULL;
  if(relative){
    sprintf(fname,RESPATH"/%s",filename.c_str());
    fp = NEW std::ifstream(fname);
  }
  else
    fp = NEW std::ifstream(filename.c_str());

  int result = 0;
  if(fp){
    if(*fp) result = 1;
    fp->close();
    delete fp;
  }

  return result;
}

int WResourceManager::CreateTexture(const string &textureName) {
	int id = INVALID_ID;
  map<string, int>::iterator itr = mTextureMap.find(textureName);

	if (itr == mTextureMap.end())
	{
		string path = graphicsFile(textureName);

		printf("creating texture:%s\n", path.c_str());

		JTexture *tex = JRenderer::GetInstance()->LoadTexture(path.c_str());

		if (tex != NULL)
    {
		id = mTextureList.size();
		mTextureList.push_back(tex);
		mTextureMap[textureName] = id;
    }
    else
    {

    }

    return id;
	}
	else
		return itr->second;
}

int WResourceManager::LoadJLBFont(const string &fontName, int height){
  map<string, int>::iterator itr = mFontMap.find(fontName);

	if (itr == mFontMap.end())
	{
		string path = graphicsFile(fontName);

		printf("creating font:%s\n", path.c_str());

		int id = mFontList.size();
		mFontList.push_back(NEW JLBFont(path.c_str(), height, true));

		mFontMap[fontName] = id;

		return id;
	}
	else
		return itr->second;
}

int WResourceManager::LoadMusic(const string &musicName){
  	map<string, int>::iterator itr = mMusicMap.find(musicName);

	if (itr == mMusicMap.end())
	{
		string path = musicFile(musicName);

		printf("creating music:%s\n", path.c_str());

		JMusic *music = JSoundSystem::GetInstance()->LoadMusic(path.c_str());
		if (music == NULL)
			return INVALID_ID;

		int id = mMusicList.size();
		mMusicList.push_back(music);

		mMusicMap[musicName] = id;

		return id;
	}
	else
		return itr->second;
}

int WResourceManager::LoadSample(const string &sampleName){
  map<string, int>::iterator itr = mSampleMap.find(sampleName);

	if (itr == mSampleMap.end())
	{
		string path = sfxFile(sampleName);

		printf("creating sample:%s\n", path.c_str());

		JSample *sample = JSoundSystem::GetInstance()->LoadSample(path.c_str());
		if (sample == NULL)
			return INVALID_ID;

		int id = mSampleList.size();
		mSampleList.push_back(sample);

		mSampleMap[sampleName] = id;

		return id;
	}
	else
		return itr->second;
}

JMusic * WResourceManager::ssLoadMusic(const char *fileName){
  return JSoundSystem::GetInstance()->LoadMusic(musicFile(fileName).c_str());
}


//Unmodified from JResourceManager

int WResourceManager::CreateQuad(const string &quadName, const string &textureName, float x, float y, float width, float height){
  map<string, int>::iterator itr = mQuadMap.find(quadName);

	if (itr == mQuadMap.end())
	{
		JTexture *tex = GetTexture(textureName);
		if (tex == NULL)
		{
			int texId = CreateTexture(textureName);		// load texture if necessary
			tex = GetTexture(texId);
		}

		if (tex == NULL)								// no texture, no quad...
			return INVALID_ID;

		printf("creating quad:%s\n", quadName.c_str());

		int id = mQuadList.size();
    if(width == 0.0f)
      width = tex->mWidth;
    if(height == 0.0f)
      height = tex->mHeight;

		mQuadList.push_back(NEW JQuad(tex, x, y, width, height));

		mQuadMap[quadName] = id;

		return id;

	}
	else
		return itr->second;
}

void WResourceManager::Refresh(){
  map<string,WCachedTexture*>::iterator it;
  vector<JQuad*>::iterator q;
  JTexture * oldtex;

  mTextureMissing.clear();
  ClearMisses();
  ClearSamples();

  for(it = textureCache.begin();it!=textureCache.end();it++){
    if(it->second == NULL)
      continue;

    oldtex = it->second->texture;
    
    //Reload the texture.
    if(it->second->bVRAM)
      it->second->texture = JRenderer::GetInstance()->LoadTexture(graphicsFile(it->first).c_str(),TEX_TYPE_USE_VRAM,TEXTURE_FORMAT);
    else
      it->second->texture = JRenderer::GetInstance()->LoadTexture(graphicsFile(it->first).c_str(),0,TEXTURE_FORMAT);

    //This texture doesn't exist in our current theme. Either use the old one, or record cache miss.
    if(!it->second->texture){
      if(it->second->isLocked())
        it->second->texture = oldtex;
      else{
        if(oldtex)
          totalsize -= oldtex->mTexHeight * oldtex->mTexWidth;

        SAFE_DELETE(oldtex);
        SAFE_DELETE(it->second);
      }
      continue;
    }
    else{
      //Alright, log the new size.
      totalsize += it->second->texture->mTexHeight * it->second->texture->mTexWidth;
    }

    //Relink quads to new texture.
    for(q = it->second->trackedQuads.begin(); q != it->second->trackedQuads.end(); q++){
      (*q)->mTex = it->second->texture;
    }
  }

  //Now do some juggling so that managed resources also reload. 
  map<JTexture *,JTexture *> oldTextures;
  map<JTexture *,string> newNames;
  map<JTexture *,JTexture *>::iterator oldIt;
  vector<JTexture*>::iterator jtex;
  map<string, int>::iterator mapping;
  JTexture * newtex;

  //Store old mappings.
  for(mapping = mTextureMap.begin();mapping != mTextureMap.end();mapping++){
    if(oldTextures[mTextureList[mapping->second]] == NULL){
      newtex = JRenderer::GetInstance()->LoadTexture(graphicsFile(mapping->first).c_str(),0,TEXTURE_FORMAT);
      oldtex = mTextureList[mapping->second];
      if(!newtex)
        newNames[oldtex] = mapping->first;
      else{
        newNames[newtex] = mapping->first;
        JRenderer::GetInstance()->BindTexture(newtex);
      }

      oldTextures[oldtex] = newtex;
    }
  }

  //Remap quads.
  for(q = mQuadList.begin();q!=mQuadList.end();q++){
    newtex = oldTextures[(*q)->mTex];
    if(newtex != NULL)
      (*q)->mTex = newtex;
  }

  //Rebuild mTextureList and mapping.
  mTextureList.clear();
  mTextureMap.clear();
  int x = 0;
  for(oldIt = oldTextures.begin();oldIt!=oldTextures.end();oldIt++){
    
    if(oldIt->second)
      newtex = oldIt->second;
    else
      newtex = oldIt->first;
    
    mTextureList.push_back(newtex);
    mTextureMap[newNames[newtex]] = x;
    x++;
  }
  
  //Rebuild mapping.
  for(mapping = mTextureMap.begin();mapping != mTextureMap.end();mapping++){
    if(oldTextures[mTextureList[mapping->second]] == NULL)
      continue;
  }

  //Delete unused textures.
  for(oldIt = oldTextures.begin();oldIt!=oldTextures.end();oldIt++){
    if(!oldIt->second || !oldIt->first )
      continue;

    oldtex = oldIt->first;
    SAFE_DELETE(oldtex);
  }
}