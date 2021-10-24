#include <napi.h>
#include <stdio.h>
#include "Navmesh/CoreMinimal.h"
#include "Navmesh/Recast/Recast.h"
#include "Navmesh/Detour/DetourNavMesh.h"
#include "Navmesh/Detour/DetourNavMeshQuery.h"

static const int NAVMESHSET_MAGIC = 'M'<<24 | 'S'<<16 | 'E'<<8 | 'T'; //'MSET';
static const int NAVMESHSET_VERSION = 1;

struct NavMeshSetHeader
{
	int magic;
	int version;
	int numTiles;
	dtNavMeshParams params;
};

struct NavMeshTileHeader
{
	dtTileRef tileRef;
	int dataSize;
};

class Navmesh : public Napi::ObjectWrap<Navmesh> {
public:
  static Napi::Object Initialize(Napi::Env env, Napi::Object exports);

  Navmesh(const Napi::CallbackInfo& info);
  ~Navmesh();

  Napi::Value Load(const Napi::CallbackInfo& info);
  Napi::Value Save(const Napi::CallbackInfo& info);
  Napi::Value Clear(const Napi::CallbackInfo& info);
  Napi::Value FindNearestPoly(const Napi::CallbackInfo& info);
  Napi::Value FindRandomPoint(const Napi::CallbackInfo& info);
  Napi::Value FindStraightPath(const Napi::CallbackInfo& info);
private:
  dtNavMesh* m_navmesh = NULL;
  dtNavMeshQuery* m_navquery = NULL;
};

Napi::Object Navmesh::Initialize(Napi::Env env, Napi::Object exports) {
  Napi::Function NavmeshClass = DefineClass(env, "Navmesh", {
    InstanceMethod("load", &Navmesh::Load),
    InstanceMethod("save", &Navmesh::Save),
    InstanceMethod("clear", &Navmesh::Clear),
    InstanceMethod("findNearestPoly", &Navmesh::FindNearestPoly),
    InstanceMethod("findRandomPoint", &Navmesh::FindRandomPoint),
    InstanceMethod("findStraightPath", &Navmesh::FindStraightPath),
  });
  Napi::FunctionReference* constructor = new Napi::FunctionReference();
  *constructor = Napi::Persistent(NavmeshClass);
  env.SetInstanceData(constructor);
  exports.Set("Navmesh", NavmeshClass);
  return exports;
}

Navmesh::Navmesh(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<Navmesh>(info) {
}

Navmesh::~Navmesh() {
  dtFreeNavMesh(this->m_navmesh);
  this->m_navmesh = NULL;
  dtFreeNavMeshQuery(this->m_navquery);
  this->m_navquery = NULL;
}

Napi::Value Navmesh::Load(const Napi::CallbackInfo& info) {
  this->Clear(info);

  if (info.Length() < 1 ||
      info[0].IsString() == false) {
    Napi::TypeError::New(info.Env(), "arguments error").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  Napi::String filename = info[0].ToString();
  info.This().As<Napi::Object>().Set("filename", filename);
  FILE* fp = fopen(filename.Utf8Value().c_str(), "rb");

  if (!fp) {
    Napi::TypeError::New(info.Env(), "open file error").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  NavMeshSetHeader header;
  if (fread(&header, sizeof(NavMeshSetHeader), 1, fp) != 1) {
    fclose(fp);
    Napi::TypeError::New(info.Env(), "read file error").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }
  if (header.magic != NAVMESHSET_MAGIC) {
    fclose(fp);
    Napi::TypeError::New(info.Env(), "header magic error").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }
  if (header.version != NAVMESHSET_VERSION) {
    fclose(fp);
    Napi::TypeError::New(info.Env(), "header version error").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  this->m_navmesh = dtAllocNavMesh();
  if (!this->m_navmesh) {
    fclose(fp);
    Napi::TypeError::New(info.Env(), "memory alloc error").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }
  dtStatus status = this->m_navmesh->init(&header.params);
  if (dtStatusFailed(status)) {
    fclose(fp);
    Napi::TypeError::New(info.Env(), "navmesh init error").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  for (int index = 0; index < header.numTiles; ++index) {
    NavMeshTileHeader tileHeader;
    if (fread(&tileHeader, sizeof(NavMeshTileHeader), 1, fp) != 1) {
      fclose(fp);
      Napi::TypeError::New(info.Env(), "read file error").ThrowAsJavaScriptException();
      return info.Env().Undefined();
    }
    if (!tileHeader.tileRef || !tileHeader.dataSize) {
      break;
    }
    uint8_t* data = (uint8_t*)dtAlloc(tileHeader.dataSize, DT_ALLOC_PERM);
    if (!data) {
      break;
    }
    memset(data, 0, tileHeader.dataSize);
    if (fread(data, tileHeader.dataSize, 1, fp) != 1) {
      dtFree(data);
      fclose(fp);
      Napi::TypeError::New(info.Env(), "read file error").ThrowAsJavaScriptException();
      return info.Env().Undefined();
    }
    this->m_navmesh->addTile(data, tileHeader.dataSize, DT_TILE_FREE_DATA, tileHeader.tileRef, 0);
  }

  fclose(fp);
  this->m_navquery = dtAllocNavMeshQuery();
  this->m_navquery->init(this->m_navmesh, 2048);
  return info.Env().Undefined();
}

Napi::Value Navmesh::Save(const Napi::CallbackInfo& info) {
  if (info.Length() < 1 ||
      info[0].IsString() == false) {
    Napi::TypeError::New(info.Env(), "arguments error").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }
  if (!this->m_navmesh) {
    Napi::TypeError::New(info.Env(), "not load").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  std::string filename = info[0].ToString().Utf8Value();

  FILE* fp = fopen(filename.c_str(), "wb");
  if (!fp) {
    Napi::TypeError::New(info.Env(), "open file error").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }

  if (filename.compare(filename.length()-4, 4, ".obj") == 0) {
    int baseVerticesLine = 1;
    for (int index = 0; index < this->m_navmesh->getMaxTiles(); ++index) {
      const dtMeshTile* tile = ((const dtNavMesh*)this->m_navmesh)->getTile(index);
      if (!tile || !tile->header || !tile->dataSize) continue;

      for (int cursor = 0; cursor < tile->header->vertCount; ++cursor) {
        const float* vertices = &tile->verts[cursor*3];
        fprintf(fp, "v %f %f %f\n", vertices[0], vertices[1], vertices[2]);
      }
      for (int cursor = 0; cursor < tile->header->detailVertCount; ++cursor) {
        const float* vertices = &tile->detailVerts[cursor*3];
        fprintf(fp, "v %f %f %f\n", vertices[0], vertices[1], vertices[2]);
      }

      for (int cursor = 0; cursor < tile->header->polyCount; ++cursor) {
        const dtPoly* poly = &tile->polys[cursor];
        const dtPolyDetail* polyDetail = &tile->detailMeshes[cursor];
        for (int j = 0; j < polyDetail->triCount; ++j) {
          const uint8_t* tris = &tile->detailTris[(polyDetail->triBase+j)*4];
          fprintf(fp, "f");
          for (int k = 0; k < 3; ++k) {
            if (tris[k] < poly->vertCount) {
              fprintf(fp, " %d", baseVerticesLine + poly->verts[tris[k]]);
            } else {
              fprintf(fp, " %d", baseVerticesLine + polyDetail->vertBase + tris[k] - poly->vertCount + tile->header->vertCount);
            }
          }
          fprintf(fp, "\n");
        }
      }

      fprintf(fp, "\n");
      baseVerticesLine += tile->header->vertCount + tile->header->detailVertCount;
    }
  } else {
    NavMeshSetHeader header;
    header.magic = NAVMESHSET_MAGIC;
    header.version = NAVMESHSET_VERSION;
    header.numTiles = 0;
    for (int index = 0; index < this->m_navmesh->getMaxTiles(); ++index) {
      const dtMeshTile* tile = ((const dtNavMesh*)this->m_navmesh)->getTile(index);
      if (!tile || !tile->header || !tile->dataSize) continue;
      header.numTiles++;
    }
    memcpy(&header.params, this->m_navmesh->getParams(), sizeof(dtNavMeshParams));
    fwrite(&header, sizeof(NavMeshSetHeader), 1, fp);

    for (int index = 0; index < this->m_navmesh->getMaxTiles(); ++index) {
      const dtMeshTile* tile = ((const dtNavMesh*)this->m_navmesh)->getTile(index);
      if (!tile || !tile->header || !tile->dataSize) continue;
      NavMeshTileHeader tileHeader;
      tileHeader.tileRef = this->m_navmesh->getTileRef(tile);
      tileHeader.dataSize = tile->dataSize;
      fwrite(&tileHeader, sizeof(tileHeader), 1, fp);
      fwrite(tile->data, tile->dataSize, 1, fp);
    }
  }

  fclose(fp);
  return info.Env().Undefined();
}

Napi::Value Navmesh::Clear(const Napi::CallbackInfo& info) {
  dtFreeNavMesh(this->m_navmesh);
  this->m_navmesh = NULL;
  dtFreeNavMeshQuery(this->m_navquery);
  this->m_navquery = NULL;
  info.This().As<Napi::Object>().Delete("filename");
  return info.Env().Undefined();
}

Napi::Value Navmesh::FindNearestPoly(const Napi::CallbackInfo& info) {
  if (info.Length() < 2 ||
      info[0].IsObject() == false ||
      info[1].IsObject() == false) {
    return info.Env().Null();
  }

  const Napi::Object centerObject = info[0].ToObject();
  const Napi::Object extentsObject = info[1].ToObject();
  const float center[] = {
    centerObject.Get("x").ToNumber().FloatValue(),
    centerObject.Get("y").ToNumber().FloatValue(),
    centerObject.Get("z").ToNumber().FloatValue(),
  };
  const float extents[] = {
    extentsObject.Get("x").ToNumber().FloatValue(),
    extentsObject.Get("y").ToNumber().FloatValue(),
    extentsObject.Get("z").ToNumber().FloatValue(),
  };
  float nearestPt[3];
  dtPolyRef nearestRef;
  dtQueryFilter filter;
  dtStatus status = this->m_navquery->findNearestPoly(center, extents, &filter, &nearestRef, nearestPt);
  if (dtStatusFailed(status) || nearestRef == 0) {
    return info.Env().Null();
  }
  Napi::Object resultObject = Napi::Object::New(info.Env());
  resultObject.Set("x", nearestPt[0]);
  resultObject.Set("y", nearestPt[1]);
  resultObject.Set("z", nearestPt[2]);
  resultObject.Set("ref", Napi::BigInt::New(info.Env(), nearestRef));
  return resultObject;
}

Napi::Value Navmesh::FindRandomPoint(const Napi::CallbackInfo& info) {
  dtQueryFilter filter;
  dtPolyRef randomRef;
  float randomPt[3];
  auto frand = []()->float {
    return std::rand() / (float)RAND_MAX;
  };
  dtStatus status;
  if (info.Length() == 0) {
    status = this->m_navquery->findRandomPoint(&filter, frand, &randomRef, randomPt);
  } else if (info.Length() == 2 &&
             info[0].IsObject() &&
             info[1].IsNumber()) {
    Napi::Object centerObject = info[0].ToObject();
    bool lossless;
    const float centerPos[] = {
      centerObject.Get("x").ToNumber().FloatValue(),
      centerObject.Get("y").ToNumber().FloatValue(),
      centerObject.Get("z").ToNumber().FloatValue(),
    };
    const dtPolyRef startRef = centerObject.Get("ref").As<Napi::BigInt>().Uint64Value(&lossless);
    const float maxRadius = info[1].ToNumber().FloatValue();
    status = this->m_navquery->findRandomPointAroundCircle(startRef, centerPos, maxRadius, &filter, frand, &randomRef, randomPt);
  } else {
    Napi::TypeError::New(info.Env(), "arguments error").ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }
  if (dtStatusFailed(status) || randomRef == 0) {
    return info.Env().Null();
  }
  Napi::Object resultObject = Napi::Object::New(info.Env());
  resultObject.Set("x", randomPt[0]);
  resultObject.Set("y", randomPt[1]);
  resultObject.Set("z", randomPt[2]);
  resultObject.Set("ref", Napi::BigInt::New(info.Env(), randomRef));
  return resultObject;
}

Napi::Value Navmesh::FindStraightPath(const Napi::CallbackInfo& info) {
  if (info.Length() < 2 ||
      info[0].IsObject() == false ||
      info[1].IsObject() == false) {
    return info.Env().Null();
  }

  const Napi::Object startObject = info[0].ToObject();
  const Napi::Object endObject = info[1].ToObject();
  bool lossless;
  const float startPos[] = {
    startObject.Get("x").ToNumber().FloatValue(),
    startObject.Get("y").ToNumber().FloatValue(),
    startObject.Get("z").ToNumber().FloatValue(),
  };
  const dtPolyRef startRef = startObject.Get("ref").As<Napi::BigInt>().Uint64Value(&lossless);
  const float endPos[] = {
    endObject.Get("x").ToNumber().FloatValue(),
    endObject.Get("y").ToNumber().FloatValue(),
    endObject.Get("z").ToNumber().FloatValue(),
  };
  const dtPolyRef endRef = endObject.Get("ref").As<Napi::BigInt>().Uint64Value(&lossless);
  dtQueryFilter filter;
  dtQueryResult findPathResult;
  float totalCost[] = {0.f};
  dtStatus status;
  status = this->m_navquery->findPath(startRef, endRef, startPos, endPos, &filter, findPathResult, totalCost);
  if (dtStatusFailed(status)) {
    return info.Env().Null();
  }
  std::vector<dtPolyRef> path;
  for (int index = 0; index < findPathResult.size(); ++index) {
    path.push_back(findPathResult.getRef(index));
  }
  dtQueryResult findStraightPathResult;
  status = this->m_navquery->findStraightPath(startPos, endPos, path.data(), path.size(), findStraightPathResult);
  if (dtStatusFailed(status)) {
    return info.Env().Null();
  }
  Napi::Array resultArray = Napi::Array::New(info.Env(), findStraightPathResult.size());
  for (int index = 0; index < findStraightPathResult.size(); index++) {
    Napi::Object resultObject = Napi::Object::New(info.Env());
    resultObject.Set("x", findStraightPathResult.getPos(index)[0]);
    resultObject.Set("y", findStraightPathResult.getPos(index)[1]);
    resultObject.Set("z", findStraightPathResult.getPos(index)[2]);
    resultObject.Set("ref", Napi::BigInt::New(info.Env(), findPathResult.getRef(index)));
    resultObject.Set("flag", findStraightPathResult.getFlag(index));
    resultObject.Set("cost", findPathResult.getCost(index));
    resultArray[index] = resultObject;
  }
  return resultArray;
}

Napi::Object Initialize(Napi::Env env, Napi::Object exports) {
  std::srand(std::time(nullptr));
  Navmesh::Initialize(env, exports);
  return exports;
}

NODE_API_MODULE(navmesh, Initialize)