#pragma once

#include <string>
#include <vector>
#include <memory>
#include "../../relive_lib/Collisions.hpp"
#include "../../relive_lib/GameType.hpp"

#include "../../Tools/relive_api/TlvsRelive.hpp"

#include "EditorCamera.hpp"
#include "CollisionObject.hpp"

class Model final
{
public:
    class ModelException
    {
    public:
        virtual ~ModelException()
        { }
        ModelException() = default;
        explicit ModelException(const std::string& what)
            : mWhat(what)
        { }
        const std::string& what() const
        {
            return mWhat;
        }

    protected:
        std::string mWhat;
    };

    // Error opening json file on disk
    class IOReadException final : public ModelException
    {
    public:
        using ModelException::ModelException;
    };

    // Json data failed to parse
    class InvalidJsonException final : public ModelException
    { };

    // Game name in the json isn't AO or AE
    class InvalidGameException final : public ModelException
    {
    public:
        using ModelException::ModelException;
    };

    // In the json schema the "typeName" couldn't be found in basic or enum types for "structureName"
    class ObjectPropertyTypeNotFoundException final : public ModelException
    {
    public:
        explicit ObjectPropertyTypeNotFoundException(const std::string& structureName, const std::string& typeName)
            : ModelException(structureName + ":" + typeName)
            , mStructName(structureName)
            , mTypeName(typeName)
        {
        }

        const std::string& StructName() const
        {
            return mStructName;
        }
        const std::string& TypeName() const
        {
            return mTypeName;
        }

    private:
        std::string mStructName;
        std::string mTypeName;
    };

    // Expected to read "key" in the json but it either didn't exist or was the wrong type
    class JsonKeyNotFoundException final : public ModelException
    {
    public:
        explicit JsonKeyNotFoundException(const std::string& key)
            : ModelException(key)
            , mKey(key)
        {
        }

        const std::string& Key() const
        {
            return mKey;
        }

    private:
        std::string mKey;
    };

    struct Enum final
    {
        std::string mName;
        std::vector<std::string> mValues;
    };
    using UP_Enum = std::unique_ptr<Enum>;


    void LoadJsonFromString(const std::string& json);
    void LoadJsonFromFile(const std::string& jsonFile);
    void CreateAsNewPath(int newPathId);
    std::string ToJson() const;

    EditorCamera* GetContainingCamera(MapObjectBase* pMapObject);

    std::unique_ptr<MapObjectBase> TakeFromContainingCamera(MapObjectBase* pMapObject);

    std::unique_ptr<EditorCamera> RemoveCamera(EditorCamera* pCamera);
    void AddCamera(UP_Camera pCamera);

    void SwapContainingCamera(MapObjectBase* pMapObject, EditorCamera* pTargetCamera);

    const std::vector<UP_Camera>& GetCameras() const 
    {
        return mCameras; 
    }

    EditorCamera* CameraAt(int x, int y) const
    {
        for (auto& cam : mCameras)
        {
            if (cam->mX == x && cam->mY == y)
            {
                return cam.get();
            }
        }
        return nullptr;
    }

    std::vector<UP_CollisionObject>& CollisionItems()
    {
        return mCollisions;
    }

    UP_CollisionObject RemoveCollisionItem(CollisionObject* pItem);

    int NextCollisionId() const
    {
        int biggestId = 0;
        for (auto& item : mCollisions)
        {
            if (item->mId > biggestId)
            {
                biggestId = item->mId;
            }
        }
        return biggestId + 1;
    }

    int IndexOfCollisionId(int id) const
    {
        if (id == -1)
        {
            return -1;
        }

        for (size_t i=0; i<mCollisions.size(); i++)
        {
            if (mCollisions[i]->mId == id)
            {
                return static_cast<int>(i);
            }
        }

        // Id wasn't found, bad input json ?
        return -1;
    }

    void SetXSize(u32 xsize)
    {
        mXSize = xsize;
    }

    void SetYSize(u32 ysize)
    {
        mYSize = ysize;
    }

    u32 XSize() const
    {
        return mXSize;
    }

    u32 YSize() const
    {
        return mYSize;
    }

    u32 CameraGridWidth() const
    {
        return mGame == GameType::eAo ? 1024 : 375;
    }

    u32 CameraGridHeight() const
    {
        return mGame == GameType::eAo ? 480 : 260;
    }

    GameType Game() const
    {
        return mGame;
    }
    s32 GetPathId() const
    {
        return mPathId;
    }

private:
    void CreateEmptyCameras();
    void CalculateMapSize();

    std::vector<UP_Camera> mCameras;
    std::vector<UP_CollisionObject> mCollisions;

    // Not part of json data, calculated on load
    u32 mXSize = 0;
    u32 mYSize = 0;

    GameType mGame = GameType::eAo;
    s32 mPathId;
    s32 mPathVersion;
    nlohmann::json mSoundInfo;
};
using UP_Model = std::unique_ptr<Model>;
