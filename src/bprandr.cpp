/*
 * BPRandr - minimal xrandr front-end (FLTK 1.3)
 *
 * bprandr          open the GUI
 * bprandr --auto   run "xrandr --auto" and exit
 */

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Box.H>
#include <FL/fl_ask.H>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <string>
#include <vector>

using std::string;
using std::vector;

struct Mode {
    string res;
    string label;
    vector<string> rates;
};

struct Mon {
    string name;
    bool internal;
    vector<Mode> modes;
    int cur_mode, cur_rate;
    int x, y, w, h, rot;
    Fl_Check_Button* on;
    Fl_Choice *pos, *res, *rate, *rotc;
    Mon() : internal(false), cur_mode(-1), cur_rate(-1), x(0), y(0), w(0),
            h(0), rot(0), on(0), pos(0), res(0), rate(0), rotc(0) {}
};

static vector<Mon*> mons;
static Fl_Double_Window* win = 0;
static Fl_Choice* preset = 0;

static const char* ROT_NAME[] = { "normal", "left", "inverted", "right" };

static bool active(const Mon* m) { return m->cur_mode >= 0 || m->w > 0; }

static bool run(const string& cmd, string* out, bool merge = true)
{
    string c = merge ? cmd + " 2>&1" : cmd;
    FILE* f = popen(c.c_str(), "r");
    if (!f) return false;
    char buf[512];
    string s;
    while (fgets(buf, sizeof buf, f)) s += buf;
    int st = pclose(f);
    if (out) *out = s;
    return st == 0;
}

static vector<string> split(const string& s)
{
    vector<string> v;
    size_t i = 0, n = s.size();
    while (i < n) {
        while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r')) i++;
        size_t j = i;
        while (j < n && s[j] != ' ' && s[j] != '\t' && s[j] != '\r') j++;
        if (j > i) v.push_back(s.substr(i, j - i));
        i = j;
    }
    return v;
}

static string with_ratio(const string& res)
{
    int w = 0, h = 0;
    if (sscanf(res.c_str(), "%dx%d", &w, &h) != 2 || w <= 0 || h <= 0)
        return res;
    int a = w, b = h;
    while (b) { int t = a % b; a = b; b = t; }
    int rw = w / a, rh = h / a;
    if (rw == 8 && rh == 5) { rw = 16; rh = 10; }

    static const int K[][2] = { {5,4}, {4,3}, {3,2}, {16,10}, {16,9},
                                {21,9}, {32,9}, {0,0} };
    bool exact = true;
    for (int i = 0; K[i][0]; i++) {
        if (rw == K[i][0] && rh == K[i][1]) break;
        double d = (double)rw / rh - (double)K[i][0] / K[i][1];
        if (d < 0) d = -d;
        if (d <= 0.01) { rw = K[i][0]; rh = K[i][1]; exact = false; break; }
    }
    char b2[128];
    snprintf(b2, sizeof b2, "%s (%s%d:%d)", res.c_str(), exact ? "" : "~", rw, rh);
    return b2;
}

static bool parse(vector<Mon*>& out)
{
    string txt;
    if (!run("LC_ALL=C xrandr -q 2>/dev/null", &txt, false)) return false;

    Mon* cur = 0;
    size_t p = 0;
    while (p < txt.size()) {
        size_t e = txt.find('\n', p);
        if (e == string::npos) e = txt.size();
        string ln = txt.substr(p, e - p);
        p = e + 1;
        if (ln.empty()) continue;

        if (ln[0] != ' ' && ln[0] != '\t') {
            /* NAME connected [primary] WxH+X+Y [rotation] (...) */
            cur = 0;
            size_t sp = ln.find(' ');
            if (sp == string::npos || ln.compare(sp + 1, 9, "connected") != 0)
                continue;
            cur = new Mon;
            cur->name = ln.substr(0, sp);
            const string& n = cur->name;
            cur->internal = n.compare(0, 3, "eDP") == 0 ||
                            n.compare(0, 4, "LVDS") == 0 ||
                            n.compare(0, 5, "PANEL") == 0 ||
                            n.compare(0, 3, "DSI") == 0;

            vector<string> t = split(ln.substr(sp + 10));
            for (size_t i = 0; i < t.size(); i++) {
                const string& s = t[i];
                if (s[0] == '(') break;
                int w, h, x, y;
                if (sscanf(s.c_str(), "%dx%d+%d+%d", &w, &h, &x, &y) == 4) {
                    cur->w = w; cur->h = h; cur->x = x; cur->y = y;
                }
                else if (s == "left")     cur->rot = 1;
                else if (s == "inverted") cur->rot = 2;
                else if (s == "right")    cur->rot = 3;
            }
            out.push_back(cur);
        }
        else if (cur) {
            /* mode line: "   1920x1080  60.00*+ 59.94" */
            vector<string> t = split(ln);
            if (t.empty() || !isdigit((unsigned char)t[0][0])) continue;

            Mode m;
            m.res = t[0];
            m.label = with_ratio(m.res);
            int idx = (int)cur->modes.size();

            for (size_t i = 1; i < t.size(); i++) {
                bool star = t[i].find('*') != string::npos;
                string r;
                for (size_t k = 0; k < t[i].size(); k++)
                    if (t[i][k] != '*' && t[i][k] != '+') r += t[i][k];
                if (!r.empty()) m.rates.push_back(r);
                if (star && !m.rates.empty()) {
                    cur->cur_mode = idx;
                    cur->cur_rate = (int)m.rates.size() - 1;
                }
            }
            cur->modes.push_back(m);
        }
    }
    return true;
}

static void free_mons(vector<Mon*>& v)
{
    for (size_t i = 0; i < v.size(); i++) delete v[i];
    v.clear();
}

static bool scan()
{
    free_mons(mons);
    return parse(mons) && !mons.empty();
}

static int count_on()
{
    int n = 0;
    for (size_t i = 0; i < mons.size(); i++)
        if (mons[i]->on->value()) n++;
    return n;
}

/* the built-in panel anchors the layout when it is enabled */
static Mon* refmon()
{
    Mon* first = 0;
    for (size_t i = 0; i < mons.size(); i++) {
        Mon* m = mons[i];
        if (!m->on->value()) continue;
        if (m->internal) return m;
        if (!first) first = m;
    }
    return first;
}

static void refresh()
{
    Mon* r = refmon();
    int n = count_on();
    for (size_t i = 0; i < mons.size(); i++) {
        Mon* m = mons[i];
        bool on = m->on->value() != 0;
        if (on && m != r && n > 1) m->pos->activate(); else m->pos->deactivate();
        if (on) { m->res->activate(); m->rate->activate(); m->rotc->activate(); }
        else    { m->res->deactivate(); m->rate->deactivate(); m->rotc->deactivate(); }
    }
}

static void fill_rates(Mon* m)
{
    m->rate->clear();
    m->rate->add("Auto");
    int mi = m->res->value() - 1;
    if (mi >= 0 && mi < (int)m->modes.size())
        for (size_t i = 0; i < m->modes[mi].rates.size(); i++)
            m->rate->add(m->modes[mi].rates[i].c_str());
    m->rate->value(0);
}

static void on_toggle(Fl_Widget*, void*) { refresh(); }
static void on_res(Fl_Widget*, void* d) { fill_rates((Mon*)d); }

static void pair_ab(Mon*& a, Mon*& b)
{
    a = mons[0]->internal ? mons[0] : (mons[1]->internal ? mons[1] : mons[0]);
    b = (a == mons[0]) ? mons[1] : mons[0];
}

static void set_max(Mon* m)
{
    m->res->value(m->modes.empty() ? 0 : 1);
    fill_rates(m);
    m->pos->value(0);
}

static void on_preset(Fl_Widget*, void*)
{
    int o = preset->value();
    if (o == 0 || mons.size() != 2) return;
    Mon *a, *b;
    pair_ab(a, b);
    set_max(a);
    set_max(b);
    a->on->value(o != 2);
    b->on->value(o != 3);
    b->pos->value(o == 4 ? 1 : (o == 5 ? 2 : 0));
    preset->value(0);
    refresh();
}

static string build_cmd()
{
    static const char* POSOPT[] = { " --same-as ", " --right-of ", " --above ",
                                    " --left-of ", " --below " };
    Mon* ref = refmon();
    int n = count_on();
    string c = "xrandr";

    for (size_t i = 0; i < mons.size(); i++) {
        Mon* m = mons[i];
        c += " --output " + m->name;
        if (!m->on->value()) { c += " --off"; continue; }

        int mi = m->res->value() - 1;
        if (mi < 0 || mi >= (int)m->modes.size()) {
            c += " --auto";
        } else {
            c += " --mode " + m->modes[mi].res;
            int ri = m->rate->value() - 1;
            /* virtual displays report 0 Hz, xrandr rejects that */
            if (ri >= 0 && ri < (int)m->modes[mi].rates.size() &&
                atof(m->modes[mi].rates[ri].c_str()) > 0.0)
                c += " --rate " + m->modes[mi].rates[ri];
        }
        c += " --rotate ";
        c += ROT_NAME[m->rotc->value()];

        if (m == ref)
            c += " --pos 0x0 --primary";
        else if (n > 1) {
            c += POSOPT[m->pos->value()];
            c += ref->name;
        }
    }
    return c;
}

/* absolute positions, so the undo doesn't depend on relative placement */
static string revert_cmd(const vector<Mon*>& v)
{
    int act = 0;
    for (size_t i = 0; i < v.size(); i++) if (active(v[i])) act++;
    if (!act) return "xrandr --auto";

    string c = "xrandr";
    for (size_t i = 0; i < v.size(); i++) {
        const Mon* m = v[i];
        c += " --output " + m->name;
        if (!active(m)) { c += " --off"; continue; }
        if (m->cur_mode >= 0) {
            c += " --mode " + m->modes[m->cur_mode].res;
            if (m->cur_rate >= 0 &&
                atof(m->modes[m->cur_mode].rates[m->cur_rate].c_str()) > 0.0)
                c += " --rate " + m->modes[m->cur_mode].rates[m->cur_rate];
        }
        char b[64];
        snprintf(b, sizeof b, " --pos %dx%d --rotate %s", m->x, m->y, ROT_NAME[m->rot]);
        c += b;
    }
    return c;
}

struct Conf { Fl_Window* w; Fl_Box* lab; int left; int ok; };

static void conf_tick(void* d)
{
    Conf* c = (Conf*)d;
    if (--c->left <= 0) { c->ok = 0; c->w->hide(); return; }
    char b[96];
    snprintf(b, sizeof b, "Keep these settings? Reverting in %d s", c->left);
    c->lab->copy_label(b);
    c->w->redraw();
    Fl::repeat_timeout(1.0, conf_tick, d);
}
static void conf_ok(Fl_Widget*, void* d) { Conf* c = (Conf*)d; c->ok = 1; c->w->hide(); }
static void conf_no(Fl_Widget*, void* d) { Conf* c = (Conf*)d; c->ok = 0; c->w->hide(); }

static bool confirm()
{
    Conf c;
    c.left = 15;
    c.ok = 0;
    Fl_Window* w = new Fl_Window(300, 90, "Confirm");
    c.w = w;
    c.lab = new Fl_Box(10, 10, 280, 30, "Keep these settings? Reverting in 15 s");
    Fl_Button* ok = new Fl_Button(40, 50, 100, 28, "Keep");
    Fl_Button* no = new Fl_Button(160, 50, 100, 28, "Revert");
    ok->callback(conf_ok, &c);
    no->callback(conf_no, &c);
    w->end();
    w->set_modal();
    w->show();
    Fl::add_timeout(1.0, conf_tick, &c);
    while (w->shown()) Fl::wait();
    Fl::remove_timeout(conf_tick, &c);
    delete w;
    return c.ok != 0;
}

static void build_ui();

static void rebuild_cb(void*)
{
    if (scan()) build_ui();
    else fl_alert("%s", "Could not read monitor information.");
}

static void do_apply(Fl_Widget*, void*)
{
    if (!count_on()) {
        fl_alert("%s", "You cannot turn off all monitors.");
        return;
    }
    vector<Mon*> old;
    if (!parse(old)) { fl_alert("%s", "Could not read the current state."); return; }
    string undo = revert_cmd(old);
    free_mons(old);

    string out;
    if (!run(build_cmd(), &out)) {
        fl_alert("xrandr failed:\n%s", out.c_str());
        run(undo, 0);
        Fl::add_timeout(0.0, rebuild_cb);
        return;
    }
    if (!confirm()) {
        run(undo, 0);
        Fl::add_timeout(0.0, rebuild_cb);
    }
}

static void on_auto(Fl_Widget*, void*)
{
    run("xrandr --auto", 0);
    Fl::add_timeout(0.3, rebuild_cb);
}

/* make sure the saved script is covered by the TinyCore backup list */
static string persist(const string& path)
{
    if (access("/usr/bin/filetool.sh", X_OK) != 0) return "";

    string rel = path;
    if (!rel.empty() && rel[0] == '/') rel = rel.substr(1);

    bool covered = false;
    FILE* f = fopen("/opt/.filetool.lst", "r");
    if (f) {
        char b[512];
        while (fgets(b, sizeof b, f)) {
            string l = b;
            while (!l.empty() && (l[l.size()-1] == '\n' || l[l.size()-1] == '\r' ||
                                  l[l.size()-1] == ' '  || l[l.size()-1] == '/'))
                l.erase(l.size() - 1);
            if (!l.empty() && l[0] == '/') l = l.substr(1);
            if (l.empty()) continue;
            if (rel == l || (rel.size() > l.size() &&
                             rel.compare(0, l.size(), l) == 0 &&
                             rel[l.size()] == '/'))
                covered = true;
        }
        fclose(f);
    }
    if (!covered) {
        f = fopen("/opt/.filetool.lst", "a");
        if (f) { fprintf(f, "%s\n", rel.c_str()); fclose(f); }
    }

    string out;
    if (run("filetool.sh -b", &out)) return "\nTinyCore backup done (filetool.sh -b).";
    return "\nWarning: filetool.sh -b failed:\n" + out;
}

static void do_save(Fl_Widget*, void*)
{
    if (!count_on()) {
        fl_alert("%s", "You cannot turn off all monitors.");
        return;
    }
    const char* home = getenv("HOME");
    if (!home || !*home) home = "/home/tc";

    string dir = string(home) + "/.X.d";
    mkdir(dir.c_str(), 0755);
    string path = dir + "/bprandr.sh";

    FILE* f = fopen(path.c_str(), "w");
    if (!f) { fl_alert("Could not write %s", path.c_str()); return; }
    fprintf(f, "#!/bin/sh\n# BPRandr saved layout\n%s || xrandr --auto\n",
            build_cmd().c_str());
    fclose(f);
    chmod(path.c_str(), 0755);

    string extra = persist(path);
    fl_message("Configuration saved to:\n%s%s", path.c_str(), extra.c_str());
}

static void on_close(Fl_Widget*, void*) { win->hide(); }

static int guess_pos(const Mon* m, const Mon* r)
{
    if (m->x >= r->x + r->w) return 1;
    if (m->x + m->w <= r->x) return 3;
    if (m->y + m->h <= r->y) return 2;
    if (m->y >= r->y + r->h) return 4;
    return 0;
}

static void build_ui()
{
    win->clear();
    win->resizable(0);      /* otherwise FLTK rescales the children on size() */
    win->begin();
    preset = 0;
    int y = 6;

    if (mons.size() == 2) {
        Mon *a, *b;
        pair_ab(a, b);
        char buf[256];
        preset = new Fl_Choice(8, y, 464, 24);
        preset->add("Quick options...");
        preset->add("Mirror on both");
        snprintf(buf, sizeof buf, "%s only", b->name.c_str());
        preset->add(buf);
        snprintf(buf, sizeof buf, "%s only", a->name.c_str());
        preset->add(buf);
        snprintf(buf, sizeof buf, "%s right of %s", b->name.c_str(), a->name.c_str());
        preset->add(buf);
        snprintf(buf, sizeof buf, "%s above %s", b->name.c_str(), a->name.c_str());
        preset->add(buf);
        preset->value(0);
        preset->callback(on_preset);
        y += 32;
    }

    Mon* r0 = 0;
    for (size_t i = 0; i < mons.size(); i++) {
        if (!active(mons[i])) continue;
        if (mons[i]->internal) { r0 = mons[i]; break; }
        if (!r0) r0 = mons[i];
    }

    for (size_t i = 0; i < mons.size(); i++) {
        Mon* m = mons[i];
        char title[160];
        snprintf(title, sizeof title, " %s%s ", m->name.c_str(),
                 m->internal ? " (internal)" : "");

        int gx = 8, gy = y + 16;
        Fl_Group* g = new Fl_Group(gx, gy, 464, 70);
        g->box(FL_ENGRAVED_BOX);
        g->copy_label(title);
        g->align(FL_ALIGN_TOP_LEFT);
        g->resizable(0);

        m->on = new Fl_Check_Button(gx + 8, gy + 8, 80, 22, "Enabled");
        m->on->value(active(m) ? 1 : 0);
        m->on->callback(on_toggle, m);

        m->pos = new Fl_Choice(gx + 190, gy + 8, 200, 22, "Position");
        m->pos->add("Same as primary (clone)");
        m->pos->add("Right of primary");
        m->pos->add("Above primary");
        m->pos->add("Left of primary");
        m->pos->add("Below primary");
        m->pos->value((r0 && m != r0 && active(m)) ? guess_pos(m, r0) : 0);

        m->res = new Fl_Choice(gx + 80, gy + 38, 150, 22, "Resolution");
        m->res->add("Auto (preferred)");
        for (size_t k = 0; k < m->modes.size(); k++)
            m->res->add(m->modes[k].label.c_str());
        m->res->value(m->cur_mode >= 0 ? m->cur_mode + 1 : 0);
        m->res->callback(on_res, m);

        m->rate = new Fl_Choice(gx + 262, gy + 38, 72, 22, "Hz");
        fill_rates(m);
        if (m->cur_mode >= 0 && m->cur_rate >= 0) m->rate->value(m->cur_rate + 1);

        m->rotc = new Fl_Choice(gx + 385, gy + 38, 72, 22, "Rotate");
        m->rotc->add("Normal");
        m->rotc->add("Left");
        m->rotc->add("Inverted");
        m->rotc->add("Right");
        m->rotc->value(m->rot);

        g->end();
        y = gy + 70 + 6;
    }

    static const char* LBL[4] = { "Auto detect", "Apply", "Save", "Close" };
    Fl_Callback* CB[4] = { on_auto, do_apply, do_save, on_close };
    for (int i = 0; i < 4; i++) {
        Fl_Button* b = new Fl_Button(8 + i * 118, y + 4, 110, 28, LBL[i]);
        b->callback(CB[i]);
    }

    win->size(480, y + 4 + 28 + 8);
    win->end();
    refresh();
    win->redraw();
}

int main(int argc, char** argv)
{
    if (argc > 1 && !strcmp(argv[1], "--auto"))
        return system("xrandr --auto") == 0 ? 0 : 1;

    FL_NORMAL_SIZE = 12;

    bool ok = scan();
    if (ok) {
        /* nothing active yet: let xrandr pick the outputs */
        bool any = false;
        for (size_t i = 0; i < mons.size(); i++) if (active(mons[i])) any = true;
        if (!any) { run("xrandr --auto", 0); ok = scan(); }
    }
    if (!ok) {
        fl_alert("%s", "Could not get monitor information.\n"
                       "Check that xrandr is installed and that X is running with RandR.");
        return 1;
    }

    win = new Fl_Double_Window(480, 100, "BPRandr");
    win->xclass("bprandr");
    win->end();
    build_ui();
    win->show(argc, argv);
    return Fl::run();
}