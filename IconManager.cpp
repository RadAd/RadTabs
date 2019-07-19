#include "IconManager.h"

#include <map>
#include <vector>

#include "Utils.h"

HIMAGELIST CreateImageList()
{
    HIMAGELIST hImageList = ImageList_Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), ILC_COLOR32 | ILC_MASK, 1, 1);
    ImageList_AddIcon(hImageList, LoadIcon(NULL, IDI_APPLICATION));
    return hImageList;
}

HIMAGELIST GetImageList()
{
    static HIMAGELIST hImageList = CreateImageList();
    return hImageList;
}

struct IconIndex
{
    int index;
    HICON hIcon;
};

static std::map<HWND, IconIndex> mapIcon;
static std::vector<int> freeList;

int NextFree()
{
    if (freeList.empty())
        return -1;
    else
    {
        int i = freeList.front();
        freeList.erase(freeList.begin());
        return i;
    }
}

int GetIconIndex(HWND hWnd)
{
    HICON hIcon = GetWindowIcon(hWnd);
    if (hIcon == NULL)
        return 0;

    HIMAGELIST hImageList = GetImageList();

    std::map<HWND, IconIndex>::iterator it = mapIcon.find(hWnd);
    if (it == mapIcon.end())
    {
        //const int i = ImageList_AddIcon(hImageList, hIcon);
        const int i = ImageList_ReplaceIcon(hImageList, NextFree(), hIcon);
        mapIcon[hWnd] = IconIndex({ i, hIcon });
        return i;
    }
    else
    {
        const int i = it->second.index;
        if (it->second.hIcon != hIcon)
        {
            it->second.hIcon = hIcon;
            ImageList_ReplaceIcon(hImageList, i, hIcon);
        }
        return i;
    }
}

void DelIcon(HWND hWnd)
{
    std::map<HWND, IconIndex>::iterator it = mapIcon.find(hWnd);
    if (it != mapIcon.end())
    {
        const int i = it->second.index;
        freeList.push_back(i);
        mapIcon.erase(it);
    }
}
