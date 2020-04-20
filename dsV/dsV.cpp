//
// 画像ビューワ
//

#include "wx/wxprec.h"
#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif
#include "wx/dnd.h"
#include "wx/filename.h"
#include "wx/image.h"
#include "wx/fileconf.h"
#include "wx/stdpaths.h"
#include "wx/clipbrd.h"
#include "wx/cmdline.h"

#include "dsV.h"

IMPLEMENT_APP(dsV)

bool dsV::OnInit()
{
    if (!wxApp::OnInit())
        return false;

    wxImage::AddHandler(new wxPNGHandler);
    wxImage::AddHandler(new wxJPEGHandler);
    wxImage::AddHandler(new wxBMPHandler);

    dsVFrame* p = new dsVFrame;
    p->Show(true);
    if (!argv.IsEmpty())
        p->AppendFiles(argv);

    return true;
}

static const wxCmdLineEntryDesc cmdline_desc[] =
{
    {wxCMD_LINE_PARAM, "", "", "",
        wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL | wxCMD_LINE_PARAM_MULTIPLE},
    {wxCMD_LINE_NONE}
};

void dsV::OnInitCmdLine(wxCmdLineParser& parser)
{
    parser.SetDesc(cmdline_desc);
}

bool dsV::OnCmdLineParsed(wxCmdLineParser& parser)
{
    argv.clear();
    for (size_t i = 0; i < parser.GetParamCount(); ++i)
        argv.Add(parser.GetParam(i));

    for (auto i = argv.begin(); i != argv.end(); ++i)
        _RPTF1(_CRT_WARN, "%s\n", i->c_str());
    return true;
}

const wxWindowIDRef ID_FILE_OPEN = wxWindow::NewControlId();
const wxWindowIDRef ID_FOLDER_OPEN = wxWindow::NewControlId();
const wxWindowIDRef ID_ZOOM_500 = wxWindow::NewControlId();
const wxWindowIDRef ID_ZOOM_400 = wxWindow::NewControlId();
const wxWindowIDRef ID_ZOOM_300 = wxWindow::NewControlId();
const wxWindowIDRef ID_ZOOM_200 = wxWindow::NewControlId();
const wxWindowIDRef ID_ZOOM_100 = wxWindow::NewControlId();
const wxWindowIDRef ID_ZOOM_50 = wxWindow::NewControlId();

wxBEGIN_EVENT_TABLE(dsVFrame, wxFrame)
    EVT_MENU(wxID_EXIT, dsVFrame::OnQuit)
    EVT_PAINT(OnPaint)
    EVT_MOUSE_EVENTS(dsVFrame::OnMouseEvent)
    EVT_KEY_UP(dsVFrame::OnKeyUp)
    EVT_SIZE(dsVFrame::OnSize)
    EVT_MENU(ID_FILE_OPEN, dsVFrame::OpenFile)
    EVT_MENU(ID_FOLDER_OPEN, dsVFrame::OpenFolder)
    EVT_MENU(ID_ZOOM_500, dsVFrame::SetScale)
    EVT_MENU(ID_ZOOM_400, dsVFrame::SetScale)
    EVT_MENU(ID_ZOOM_300, dsVFrame::SetScale)
    EVT_MENU(ID_ZOOM_200, dsVFrame::SetScale)
    EVT_MENU(ID_ZOOM_100, dsVFrame::SetScale)
    EVT_MENU(ID_ZOOM_50, dsVFrame::SetScale)
wxEND_EVENT_TABLE()

dsVFrame::dsVFrame()
    : wxFrame((wxFrame*)nullptr, wxID_ANY, wxT("ファイルをドロップ"),
        wxDefaultPosition, wxDefaultSize)
{
    filenames.clear();
    select_file.Clear();
    select_file_idx = 0;
    scale = 10;
    sx = 0;
    sy = 0;
    drag_mode = DRAG_MODE::DRAG_NONE;
    ini_cfg = nullptr;

    SetDropTarget(new DnDFile(this));
    SetBackgroundColour(wxColour(0, 0, 0));

    wxFileName fullpath(wxStandardPaths::Get().GetExecutablePath());
    wxFileName ini_path;
    ini_path.AssignDir(fullpath.GetPathWithSep());
    ini_path.SetFullName("dsV.ini");

    ini_cfg = new wxFileConfig("dsV", "denchu",
        ini_path.GetFullPath(), wxEmptyString, wxCONFIG_USE_LOCAL_FILE);

    ini_cfg->SetPath(wxT("/MainFrame"));
    const int x = ini_cfg->Read(wxT("x"), (long)0);
    const int y = ini_cfg->Read(wxT("y"), (long)0);
    const int width = ini_cfg->Read(wxT("width"), (long)1024);
    const int height = ini_cfg->Read(wxT("height"), (long)768);
    Move(x, y);
    SetClientSize(width, height);
}

dsVFrame::~dsVFrame()
{
    if (ini_cfg) {
        int x, y, w, h;
        GetClientSize(&w, &h);
        GetPosition(&x, &y);
        ini_cfg->SetPath(wxT("/MainFrame"));
        ini_cfg->Write(wxT("x"), (long)x);
        ini_cfg->Write(wxT("y"), (long)y);
        ini_cfg->Write(wxT("width"), (long)w);
        ini_cfg->Write(wxT("height"), (long)h);
        delete ini_cfg;
        ini_cfg = nullptr;
    }
}

void dsVFrame::OnQuit(wxCommandEvent& ev)
{
    Close(true);
}

void dsVFrame::OnKeyUp(wxKeyEvent& ev)
{
    int key = ev.GetKeyCode();
    switch (key) {
    case 0x1b:
        PostQuitMessage(0);
        break;
    case 'Z':
        ChangeScale(1);
        break;
    case 'X':
        ChangeScale(-1);
        break;
    case '0':
        ChangeScale(0);
        break;
    case 'N':
        NextView(1);
        break;
    case 'P':
        NextView(-1);
        break;
    case 'V':
        if (ev.ControlDown())
            FromClipboard();
        break;
    case 'S':
        if (ev.ControlDown())
            SaveData();
        break;
    default:
        ev.Skip();
    }
}

void dsVFrame::OnPaint(wxPaintEvent& ev)
{
    wxPaintDC dc(this);
    if (draw_bmp.IsOk())
        dc.DrawBitmap(draw_bmp, -sx, -sy);
}

void dsVFrame::OnSize(wxSizeEvent& ev)
{
    if (!draw_bmp.IsOk()) {
        ev.Skip();
        return;
    }
    // ev.GetSize()だと、リサイズ用のサイズが入っているためちょっとまずい
    //wxSize s = ev.GetSize();
    //const int client_width = s.GetWidth();
    //const int client_height = s.GetHeight();
    // そもそもリサイズ用の枠のサイズを取得するのどうやるんだろう
    const int view_width = draw_bmp.GetWidth();
    const int view_height = draw_bmp.GetHeight();
    const wxSize client_size = GetClientSize();
    const int client_width = client_size.GetWidth();
    const int client_height = client_size.GetHeight();
    const int prev_sx = sx;
    const int prev_sy = sy;
    if (view_width - sx < client_width) {
        sx = view_width - client_width;
        if (sx < 0)
            sx = 0;
    }
    if (view_height - sy < client_height) {
        sy = view_height - client_height;
        if (sy < 0)
            sy = 0;
    }
    if (prev_sx != sx || prev_sy != sy)
        Refresh();
    ev.Skip();
}

void dsVFrame::LoadSelectImage(void)
{
    if (!select_file.IsOk())
        return;

    wxString fname = select_file.GetFullPath();
    draw_img = wxImage(fname);
}

void dsVFrame::UpdateBitmap(void)
{
    if (draw_img.IsOk()) {
        if (scale != 10) {
            const int w = draw_img.GetWidth() * scale / 10;
            const int h = draw_img.GetHeight() * scale / 10;
            wxImage img = draw_img.Scale(w, h, wxIMAGE_QUALITY_HIGH);
            if (img.IsOk())
                draw_bmp = wxBitmap(img);
        }
        else {
            draw_bmp = wxBitmap(draw_img);
        }
    }
    SetImageTitle();
}

void dsVFrame::SetImageTitle(void)
{
    wxString msg;
    if (draw_img.IsOk()) {
        int w = draw_img.GetWidth();
        int h = draw_img.GetHeight();
        if (scale != 10) {
            msg = wxString::Format(wxT("(x%.1f)"), (double)scale/10.0);
            w = w * scale / 10;
            h = h * scale / 10;
        }
        msg += wxString::Format(wxT("[%u,%u]"), w, h);
        msg += wxString::Format(wxT(":%s"), select_file.GetFullName().c_str());
    }
    else {
        msg = wxT("ファイルをドロップ");
    }
    SetTitle(msg);
}

void dsVFrame::AppendFiles(const wxArrayString& files)
{
    // リストにファイルを追加する
    wxFileName sfile;
    wxArrayString globs;
    for (wxArrayString::const_iterator p = files.begin(); p != files.end(); ++p) {
        wxFileName fn;
        if (wxDirExists(*p))
            fn.AssignDir(*p);
        else
            fn.Assign(*p);

        if (fn.Exists()) {
            if (fn.IsDir()) {
                // glob
                wxString t = fn.GetPathWithSep();
                if (globs.Index(t) == wxNOT_FOUND)
                    globs.Add(t);
            }
            else {
                wxString t = fn.GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR);
                if (globs.Index(t) == wxNOT_FOUND)
                    globs.Add(t);
                // ファイルを選択させる
                sfile = fn;
                sfile.MakeAbsolute();
            }
        }
    }
    if (!globs.IsEmpty()) {
        for (wxArrayString::iterator p = globs.begin(); p != globs.end(); ++p)
            GlobDirectory(*p);
    }
    if (!sfile.IsOk() && filenames.GetCount() > 0) {
        sfile.Assign(filenames[0]);
    }
    if (sfile.IsOk()) {
        // 一覧からファイルの位置を検索しておく
        const int idx = filenames.Index(sfile.GetFullPath(), false);
        if (idx != wxNOT_FOUND) {
            select_file = sfile;
            select_file_idx = idx;
        }
        else {
            select_file.Clear();
            select_file_idx = -1;
        }
        LoadSelectImage();
        UpdateBitmap();
        Refresh();
    }
}

void dsVFrame::AppendFile(const wxString& filename)
{
    // ファイルを追加
    wxFileName fn(filename);
    if (!fn.FileExists())
        return;
    fn.MakeAbsolute();

    wxString fullpath = fn.GetFullPath();
    if (wxImage::CanRead(fullpath))
        filenames.Add(fullpath);
}

void dsVFrame::GlobDirectory(const wxString& fn)
{
    wxString wildcard(fn);
    wildcard += "*.*";
    wxString f = wxFindFirstFile(wildcard);
    while (!f.empty()) {
        AppendFile(f);
        f = wxFindNextFile();
    }
}

void dsVFrame::OnMouseEvent(wxMouseEvent& ev)
{
    if (ev.RightUp()) {
        OpenMenu(ev.GetPosition());
    }
    else if (ev.LeftDown() && drag_mode == DRAG_MODE::DRAG_NONE) {
        drag_start_pos = ev.GetPosition();
        drag_mode = DRAG_MODE::DRAG_START;
    }
    else if (ev.LeftUp() && drag_mode != DRAG_MODE::DRAG_NONE) {
        drag_mode = DRAG_MODE::DRAG_NONE;
    }
    else if (ev.Dragging() && drag_mode != DRAG_MODE::DRAG_NONE) {
        const int dx = ev.GetPosition().x - drag_start_pos.x;
        const int dy = ev.GetPosition().y - drag_start_pos.y;
        const int prev_sx = sx;
        const int prev_sy = sy;
        const int width = draw_bmp.GetWidth();
        const int height = draw_bmp.GetHeight();
        const wxSize client_size = GetClientSize();
        const int client_width = client_size.GetWidth();
        const int client_height = client_size.GetHeight();
        sx -= dx;
        if (sx + client_width > width)
            sx = width - client_width;
        if (sx < 0)
            sx = 0;
        sy -= dy;
        if (sy + client_height > height)
            sy = height - client_height;
        if (sy < 0)
            sy = 0;

        drag_start_pos = ev.GetPosition();
        drag_mode = DRAG_MODE::DRAG_DRAGGING;
        if (prev_sx != sx || prev_sy != sy)
            Refresh();
    }
    else if (ev.Leaving()) {
        // 画面範囲外にいったので初期化しておく
        if (drag_mode != DRAG_MODE::DRAG_NONE)
            drag_mode = DRAG_MODE::DRAG_NONE;
    }
    else if (ev.GetEventType() == wxEVT_MOUSEWHEEL) {
        const int delta = ev.GetWheelDelta();
        const int rotate = ev.GetWheelRotation();
        if (delta != 0) {
            const int vect = rotate / delta;
            if (vect < 0)
                ChangeScale(1);
            else
                ChangeScale(-1);
        }
    }
}

void dsVFrame::NextView(const int step)
{
    const size_t max_file_nr = filenames.GetCount();
    if (max_file_nr == 0)
        return;
    if (!draw_img.IsOk())
        return;
    const size_t prev = select_file_idx;
    select_file_idx = (select_file_idx + max_file_nr + step) % max_file_nr;
    if (select_file_idx == prev)
        return;
    select_file = filenames[select_file_idx];
    sx = 0;
    sy = 0;
    LoadSelectImage();
    UpdateBitmap();
    Refresh();
}

bool dsVFrame::IsChangeScale(const int percent)
{
    // 規定以上のサイズになったらfalseを返す
    if (!draw_img.IsOk())
        return false;
    if (draw_img.GetWidth() / 10 > MAX_DRAW_WIDTH / percent ||
        draw_img.GetHeight() / 10 > MAX_DRAW_HEIGHT / percent)
        return false;
    return true;
}

void dsVFrame::ChangeScale(const int step)
{
    if (!draw_img.IsOk())
        return;
    const int prev = scale;
    if (step == 0)
        scale = 10;
    else
        scale += step;
    if (scale < 1)
        scale = 1;
    if (!IsChangeScale(scale)) {
        scale = prev;
        return;
    }
    UpdateBitmap();
    Refresh();
}

void dsVFrame::FromClipboard(void)
{
    if (wxTheClipboard->Open()) {
        if (wxTheClipboard->IsSupported(wxDF_BITMAP)) {
            wxBitmapDataObject bmp;
            if (wxTheClipboard->GetData(bmp)) {
                wxBitmap img = bmp.GetBitmap();
                draw_img = img.ConvertToImage();
                filenames.Clear();
                select_file.Clear();
                select_file_idx = -1;
                UpdateBitmap();
                Refresh();
            }
        }
        else if (wxTheClipboard->IsSupported(wxDF_FILENAME)) {
            wxFileDataObject fn;
            if (wxTheClipboard->GetData(fn)) {
                const wxArrayString& filenames = fn.GetFilenames();
                for (wxArrayString::const_iterator p = filenames.begin();
                    p != filenames.end(); ++p) {
                    AppendFile(*p);
                }
                if (!filenames.IsEmpty()) {
                    select_file = filenames[0];
                    select_file_idx = 0;
                    LoadSelectImage();
                    UpdateBitmap();
                    Refresh();
                }
            }
        }
    }
}

void dsVFrame::OpenMenu(const wxPoint& pt)
{
    wxMenu menu;
    menu.Append(ID_FILE_OPEN, wxT("ファイルを開く(&F)"));
    menu.Append(ID_FOLDER_OPEN, wxT("フォルダを開く"));

    if (draw_bmp.IsOk()) {
        wxMenu* zoom = new wxMenu;
        zoom->Append(ID_ZOOM_500, wxT("500%"));
        zoom->Append(ID_ZOOM_400, wxT("400%"));
        zoom->Append(ID_ZOOM_300, wxT("300%"));
        zoom->Append(ID_ZOOM_200, wxT("200%"));
        zoom->AppendSeparator();
        zoom->Append(ID_ZOOM_100, wxT("100%"));
        zoom->AppendSeparator();
        zoom->Append(ID_ZOOM_50, wxT("50%"));

        menu.AppendSubMenu(zoom, wxT("拡大縮小"));
    }
    menu.AppendSeparator();
    menu.Append(wxID_EXIT, "終了(&X)");

    PopupMenu(&menu, pt);
}

void dsVFrame::OpenFile(wxCommandEvent&)
{
    wxFileDialog dlg(this, wxT("ファイルを選択してください"),
        wxEmptyString, wxEmptyString,
        wxT("PNG(*.png)|*.png|JPEG(*.jpeg)|*.jpg;*.jpeg|BMP(*.bmp)|*.bmp|ALL(*.*)|*.*"),
        wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);
    if (dlg.ShowModal() == wxID_CANCEL)
        return;
    wxArrayString fn;
    dlg.GetPaths(fn);
    if (!fn.IsEmpty())
        AppendFiles(fn);
}

void dsVFrame::OpenFolder(wxCommandEvent&)
{
    wxDirDialog dlg(this, wxT("フォルダを選択してください"),
        wxEmptyString, wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    if (dlg.ShowModal() == wxID_CANCEL)
        return;
    wxString path = dlg.GetPath();
    wxArrayString as;
    as.Add(path);
    AppendFiles(as);
}

void dsVFrame::SetScale(wxCommandEvent& ev)
{
    if (!draw_img.IsOk())
        return;

    const int prev = scale;
    const int id = ev.GetId();
    // switch文が使えない
    if (id == ID_ZOOM_500) {
        scale = 50;
    }
    else if (id == ID_ZOOM_400) {
        scale = 40;
    }
    else if (id == ID_ZOOM_300) {
        scale = 30;
    }
    else if (id == ID_ZOOM_200) {
        scale = 20;
    }
    else if (id == ID_ZOOM_100) {
        scale = 10;
    }
    else if (id == ID_ZOOM_50) {
        scale = 5;
    }
    if (!IsChangeScale(scale)) {
        scale = prev;
        return;
    }
    UpdateBitmap();
    Refresh();
}

void dsVFrame::SaveData(void)
{
    if (!draw_img.IsOk())
        return;
    wxFileDialog dlg(this, wxT("Imageを保存"), wxEmptyString, wxEmptyString,
        wxT("png file|*.png"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() == wxID_CANCEL)
        return;
    wxString filename = dlg.GetPath();
    draw_img.SaveFile(filename, wxBITMAP_TYPE_PNG);
}