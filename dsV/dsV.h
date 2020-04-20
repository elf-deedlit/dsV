//
// 画像ビューワ
//

#pragma once

class dsV:public wxApp
{
private:
    wxArrayString argv;
public:
    bool OnInit();
    void OnInitCmdLine(wxCmdLineParser&);
    bool OnCmdLineParsed(wxCmdLineParser&);
};

DECLARE_APP(dsV)

enum class DRAG_MODE {
    DRAG_NONE,
    DRAG_START,
    DRAG_DRAGGING,
};

static const int MAX_DRAW_WIDTH = 10000;
static const int MAX_DRAW_HEIGHT = 10000;

class dsVFrame : public wxFrame
{
private:
    wxPathList filenames;
    wxFileName select_file;
    int select_file_idx;
    int sx, sy;
    int scale; // 表示サイズの10倍の値
    wxImage draw_img;
    wxBitmap draw_bmp;
    DRAG_MODE drag_mode;
    wxPoint drag_start_pos;
    wxFileConfig* ini_cfg;

    void GlobDirectory(const wxString&);
    void AppendFile(const wxString&);
    void UpdateBitmap(void);
    void LoadSelectImage(void);
    void SetImageTitle(void);
    void NextView(const int step);
    void ChangeScale(const int step);
    void FromClipboard(void);
    void OpenMenu(const wxPoint&);
    void OpenFile(wxCommandEvent&);
    void OpenFolder(wxCommandEvent&);
    void SetScale(wxCommandEvent&);
    bool IsChangeScale(const int percent);
    void SaveData(void);

public:
    dsVFrame();
    ~dsVFrame();
    void OnQuit(wxCommandEvent&);
    void OnMouseEvent(wxMouseEvent&);
    void OnPaint(wxPaintEvent&);
    void OnKeyUp(wxKeyEvent&);
    void OnSize(wxSizeEvent&);

    void AppendFiles(const wxArrayString&);
    wxDECLARE_EVENT_TABLE();
};

class DnDFile : public wxFileDropTarget
{
public:
    DnDFile(dsVFrame* parent) { owner_wnd = parent; }

    virtual bool OnDropFiles(wxCoord x, wxCoord y,
        const wxArrayString& filenames) wxOVERRIDE {
        if (owner_wnd != nullptr) {
            owner_wnd->AppendFiles(filenames);
        }

        return true;
    };

private:
    dsVFrame* owner_wnd;
};

