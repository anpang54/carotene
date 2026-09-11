
#pragma once

/*

    There are 4 separate backends:
      - Windows:               Win32, linked normally
      - macOS, Linux, FreeBSD: GTK 4, with dlopen()
      - Haiku:                 BeAPI, linked normally
      - Web:                   DOM
    
    Maybe Cocoa and Qt later.

*/


// INCLUDES

#include <algorithm>
#include <functional>

#if defined(__EMSCRIPTEN__)
    #define CARO_GUI_WEB

    #include "gui-styles.hpp"

#elif defined(_WIN32)
	#define CARO_GUI_WIN32

	#define WIN32_LEAN_AND_MEAN 
	#define NOMINMAX
	#define TokenType WindowsTokenType
	#include <windows.h>
	#undef TokenType

#elif defined(__HAIKU__)
	#define CARO_GUI_BEAPI
		
	#include <Application.h>
	#include <Window.h>
	#include <StringView.h>
	#include <Button.h>
	#include <TextControl.h>
	#include <TextView.h>
	#include <ScrollView.h>
	#include <MenuField.h>
	#include <PopUpMenu.h>
	#include <MenuItem.h>
	#include <LayoutBuilder.h>
	#include <GridLayout.h>
	#include <GroupView.h>

#else
    #define CARO_GUI_GTK
	
    #include <dlfcn.h>
	
#endif

#include "../../core/format.hpp"


// ASYNCIFY

#ifdef CARO_GUI_WEB
    EM_ASYNC_JS(int, caroWaitForEvent, (), {
        if(Module.caroEvents.length == 0) await new Promise(resolve => Module.caroWake = resolve);
        return Module.caroEvents.shift();
    });
#endif


namespace CaroGui{


    // WINDOW DESCRIPTION

    // list of widgets, each one gets a defined number because the web backend can't access this enum
    enum WidgetType{

     // WIDGET_BOX           = 100,
     // WIDGET_POPUP         = 101,

     // WIDGET_HORIZONTALBAR = 200,
     // WIDGET_VERTICALBAR   = 201,
     // WIDGET_SPINNER       = 202,
     // WIDGET_LEVELBAR      = 203,

        WIDGET_LABEL         = 210,
     // WIDGET_LINK          = 211,

     // WIDGET_IMAGE         = 220,
     // WIDGET_AUDIO         = 221,
     // WIDGET_VIDEO         = 222,

        WIDGET_BUTTON        = 300,
     // WIDGET_TOGGLEBUTTON  = 301,
     // WIDGET_CHECKBOX      = 302,
     // WIDGET_RADIOBUTTON   = 303,
     // WIDGET_SWITCH        = 304,
     // WIDGET_TABS          = 305,
        
     // WIDGET_DATETIME      = 310,
     // WIDGET_COLOR         = 311,
     // WIDGET_FONT          = 312,
     // WIDGET_EMOJI         = 313,

        WIDGET_TEXTBOX       = 320,
        WIDGET_TEXTAREA      = 321,
        WIDGET_SELECT        = 322,
        WIDGET_COMBOBOX      = 323,
     // WIDGET_PASSWORD      = 324,

        // https://docs.gtk.org/gtk4/visual_index.html

    };

    struct Widget{
        WidgetType type;
        string text;
        int x     = 0, y     = 0,
            xSpan = 1, ySpan = 1;    // counts, not end position
        void* handle = nullptr;      // the native control
        vector<string> options;      // gui.Select and gui.ComboBox
        int selected = 0;
    };

    struct Window{
        string title;
        int width   = 400;
        int height  = 250;
        int spacing = 25;
        vector<Widget*> widgets;
    };

    // handlers
    typedef std::function<bool(size_t widget)> ClickHandler;
    typedef std::function<bool()> ShowHandler;


    // WEB (acrylic elements)

    #ifdef CARO_GUI_WEB

        // add gui-styles.css and the google fonts it needs
        void addStyles() {
            EM_ASM({

                if(document.getElementById("caro-gui-styles")) return;    // already has the styles

                const link1 = document.createElement("link");
                link1.rel         = "preconnect";
                link1.href        = "https://fonts.googleapis.com";
                document.head.appendChild(link1);

                const link2 = document.createElement("link");
                link2.rel         = "preconnect";
                link2.href        = "https://fonts.gstatic.com";
                link2.crossOrigin = "";
                document.head.appendChild(link2);

                const link3 = document.createElement("link");
                link3.rel         = "stylesheet";
                link3.href        = "https://fonts.googleapis.com/css2?family=Inter&family=Roboto+Mono&family=Schibsted+Grotesk&display=swap";
                document.head.appendChild(link3);

                const style = document.createElement("style");
                style.id          = "caro-gui-styles";
                style.textContent = UTF8ToString($0);
                document.head.appendChild(style);

            }, GUI_STYLES);
        }

        void close() {
            EM_ASM({
                if(!Module.caroContainer) return;
                Module.caroContainer.remove();
                Module.caroContainer = null;
                Module.caroElements = null;
                Module.caroEvents = [];
                Module.caroPush(-1);
            });
        }

        string show(const Window& window, const ClickHandler& onClick, const ShowHandler& onShow = {}) {

            if(EM_ASM_INT({ return Module.caroContainer? 1: 0; })) return "A window is already open.";

            addStyles();

            EM_ASM({

                const div = document.createElement("div");
                div.id = "caro-container";
                div.style.gap = `${$1}px`;
                document.title = UTF8ToString($0);
                document.body.appendChild(div);
                Module.caroContainer = div;
                Module.caroElements = new Map();

                // event queue
                Module.caroEvents = [];
                Module.caroPush = (event) => {
                    Module.caroEvents.push(event);
                    if(Module.caroWake) {
                        const wake = Module.caroWake;
                        Module.caroWake = null;
                        wake();
                    }
                };
                
            }, window.title.c_str(), window.spacing);

            for(size_t i = 0; i < window.widgets.size(); ++i) {

                const Widget& widget = *window.widgets[i];

                EM_ASM({

                    let element;
                    switch($0) {

                        case 210:
                            element = document.createElement("span");
                            break;

                        case 300:
                            element = document.createElement("button");
                            element.addEventListener("click", () => Module.caroPush($2));
                            break;

                        case 320:
                            element = document.createElement("input");
                            element.type = "text";
                            element.addEventListener("input", () => Module.caroPush(-2 - $2));
                            break;

                        case 321:
                            element = document.createElement("textarea");
                            element.rows = 5;
                            element.addEventListener("input", () => Module.caroPush(-2 - $2));
                            break;

                        case 322:
                            element = document.createElement("select");
                            element.addEventListener("change", () => Module.caroPush(-2 - $2));
                            break;

                        case 323: {
                            element = document.createElement("input");
                            element.type = "text";
                            const list = document.createElement("datalist");
                            list.id = `caro-combobox-${$2}`;
                            element.setAttribute("list", list.id);
                            Module.caroContainer.appendChild(list);
                            element.addEventListener("input", (event) => {
                                Module.caroPush(-2 - $2);
                                if(!event.inputType || event.inputType == "insertReplacementText") Module.caroPush($2);
                            });
                            break;
                        }

                    }

                    if($0 == 320 || $0 == 321 || $0 == 323) {
                        element.value     = UTF8ToString($1);
                    } else if($0 == 322) {
                        // do nothing
                    } else {
                        element.innerText = UTF8ToString($1);
                    }
                    
                    element.style.gridColumn = `${$3 + 1} / span ${$4}`;
                    element.style.gridRow    = `${$5 + 1} / span ${$6}`;
                    Module.caroContainer.appendChild(element);
                    Module.caroElements.set($7, element);

                }, widget.type, widget.text.c_str(), (int)i, widget.x, widget.xSpan, widget.y, widget.ySpan, &widget);

                // select/combobox options
                for(const string& option: widget.options) {
                    EM_ASM({
                        const option = document.createElement("option");
                        option.text = UTF8ToString($1);
                        const element = Module.caroElements.get($0);
                        (element.list || element).appendChild(option);
                    }, &widget, option.c_str());
                }
                if(widget.type == WIDGET_SELECT) EM_ASM({ Module.caroElements.get($0).selectedIndex = $1; }, &widget, widget.selected);

            }

            if(onShow && !onShow()) close();

            // block execution until the window gets closed
            // events: -1 = closed, -2 - i = textbox/textarea/select/combobox i changed, i = widget i clicked or combobox option picked
            while(true) {
                int event = caroWaitForEvent();
                if(event == -1) break;
                if(event <= -2) {
                    size_t index = -2 - event;
                    Widget& widget = *window.widgets[index];
                    if(widget.type == WIDGET_SELECT) {
                        widget.selected = EM_ASM_INT({ return Module.caroElements.get($0).selectedIndex; }, &widget);
                        if(!onClick(index)) close();
                        continue;
                    }
                    char* value = (char*)EM_ASM_PTR({
                        const element = Module.caroElements && Module.caroElements.get($0);
                        return element? stringToNewUTF8(element.value): 0;
                    }, &widget);
                    if(value) {
                        widget.text = value;
                        free(value);
                    }
                    continue;
                }
                if(!onClick(event)) close();
            }

            return "";

        }

        void setText(Widget& widget, const string& text) {
            widget.text = text;
            EM_ASM({
                const element = Module.caroElements && Module.caroElements.get($0);
                if(!element) return;
                if(element.tagName == "INPUT" || element.tagName == "TEXTAREA") element.value     = UTF8ToString($1);
                else                                                            element.innerText = UTF8ToString($1);
            }, &widget, text.c_str());
        }

        void setSelected(Widget& widget, int index) {
            widget.selected = index;
            EM_ASM({
                const element = Module.caroElements && Module.caroElements.get($0);
                if(element) element.selectedIndex = $1;
            }, &widget, index);
        }


    // GTK

    #elifdef CARO_GUI_GTK


        // gtk functions

        #define gtkFunctions(f)\
            \
            /* GTK */\
            f(initCheck,                 "gtk_init_check",                    int,           (void))\
            f(windowNew,                 "gtk_window_new",                    void*,         (void))\
            f(windowSetTitle,            "gtk_window_set_title",              void,          (void*, const char*))\
            f(windowSetDefaultSize,      "gtk_window_set_default_size",       void,          (void*, int, int))\
            f(windowSetChild,            "gtk_window_set_child",              void,          (void*, void*))\
            f(windowPresent,             "gtk_window_present",                void,          (void*))\
            f(windowDestroy,             "gtk_window_destroy",                void,          (void*))\
            f(labelNew,                  "gtk_label_new",                     void*,         (const char*))\
            f(labelSetText,              "gtk_label_set_text",                void,          (void*, const char*))\
            f(buttonNewWithLabel,        "gtk_button_new_with_label",         void*,         (const char*))\
            f(buttonSetLabel,            "gtk_button_set_label",              void,          (void*, const char*))\
            f(entryNew,                  "gtk_entry_new",                     void*,         (void))\
            f(editableGetText,           "gtk_editable_get_text",             const char*,   (void*))\
            f(editableSetText,           "gtk_editable_set_text",             void,          (void*, const char*))\
            f(dropDownNewFromStrings,    "gtk_drop_down_new_from_strings",    void*,         (const char* const*))\
            f(dropDownGetSelected,       "gtk_drop_down_get_selected",        unsigned int,  (void*))\
            f(dropDownSetSelected,       "gtk_drop_down_set_selected",        void,          (void*, unsigned int))\
            f(comboBoxTextNewWithEntry,  "gtk_combo_box_text_new_with_entry", void*,         (void))\
            f(comboBoxTextAppendText,    "gtk_combo_box_text_append_text",    void,          (void*, const char*))\
            f(comboBoxGetActive,         "gtk_combo_box_get_active",          int,           (void*))\
            f(comboBoxGetChild,          "gtk_combo_box_get_child",           void*,         (void*))\
            f(textViewNew,               "gtk_text_view_new",                 void*,         (void))\
            f(textViewGetBuffer,         "gtk_text_view_get_buffer",          void*,         (void*))\
            f(textViewSetWrapMode,       "gtk_text_view_set_wrap_mode",       void,          (void*, int))\
            f(textViewSetLeftMargin,     "gtk_text_view_set_left_margin",     void,          (void*, int))\
            f(textViewSetRightMargin,    "gtk_text_view_set_right_margin",    void,          (void*, int))\
            f(textViewSetTopMargin,      "gtk_text_view_set_top_margin",      void,          (void*, int))\
            f(textViewSetBottomMargin,   "gtk_text_view_set_bottom_margin",   void,          (void*, int))\
            f(textBufferSetText,         "gtk_text_buffer_set_text",          void,          (void*, const char*, int))\
            f(textBufferGetBounds,       "gtk_text_buffer_get_bounds",        void,          (void*, TextIter*, TextIter*))\
            f(textBufferGetText,         "gtk_text_buffer_get_text",          char*,         (void*, const TextIter*, const TextIter*, int))\
            f(scrolledWindowNew,         "gtk_scrolled_window_new",           void*,         (void))\
            f(scrolledWindowSetChild,    "gtk_scrolled_window_set_child",     void,          (void*, void*))\
            f(scrolledWindowSetHasFrame, "gtk_scrolled_window_set_has_frame", void,          (void*, int))\
            f(gridNew,                   "gtk_grid_new",                      void*,         (void))\
            f(gridAttach,                "gtk_grid_attach",                   void,          (void*, void*, int, int, int, int))\
            f(gridSetRowSpacing,         "gtk_grid_set_row_spacing",          void,          (void*, unsigned int))\
            f(gridSetColumnSpacing,      "gtk_grid_set_column_spacing",       void,          (void*, unsigned int))\
            f(widgetSetHalign,           "gtk_widget_set_halign",             void,          (void*, int))\
            f(widgetSetValign,           "gtk_widget_set_valign",             void,          (void*, int))\
            f(widgetSetSizeRequest,      "gtk_widget_set_size_request",       void,          (void*, int, int))\
            \
            /* GObject */\
            f(signalConnectData,         "g_signal_connect_data",             unsigned long, (void*, const char*, void(*)(), void*, void*, int))\
            \
            /* GLib */\
            f(mainContextIteration,      "g_main_context_iteration",          int,           (void*, int))\
            f(gFree,                     "g_free",                            void,          (void*))


        struct TextIter{
            alignas(void*) unsigned char data[128];
        };


        // struct

        struct GtkLibrary{

            void* gtk = nullptr;

            #define gtkField(name, symbol, ret, args) ret (*name) args;
            gtkFunctions(gtkField)
            #undef gtkField

            template<typename T>
            bool load(T& function, const char* name) {
                function = (T)dlsym(gtk, name);
                return function != nullptr;
            }

            GtkLibrary() {

                // this one dlopen() also loads the 2 dependencies
                gtk = dlopen("libgtk-4.so.1", RTLD_LAZY | RTLD_LOCAL);
                if(!gtk) return;

                #define gtkLoad(name, symbol, ret, args) && load(name, symbol)
                bool complete = true gtkFunctions(gtkLoad);
                #undef gtkLoad

                if(!complete) {
                    dlclose(gtk);
                    gtk = nullptr;
                }

            }

        };


        // load

        const GtkLibrary& gtk() {
            static GtkLibrary library;
            return library;
        }

        bool started() {
            static bool ok = gtk().initCheck() != 0;
            return ok;
        }


        // callbacks

        void* currentWindow = nullptr;

        struct Click{
            size_t widget;
            const ClickHandler* onClick;
            Widget* target = nullptr;
        };

        void onDestroy(void*, void* closed) {
            *(bool*)closed = true;
            currentWindow = nullptr;
        }
        void onClicked(void*, void* data) {
            Click* click = (Click*)data;
            if(!(*click->onClick)(click->widget) && currentWindow) gtk().windowDestroy(currentWindow);
        }
        void onChanged(void* entry, void* widget) {
            ((Widget*)widget)->text = gtk().editableGetText(entry);
        }
        void onBufferChanged(void* buffer, void* widget) {
            TextIter start, end;
            gtk().textBufferGetBounds(buffer, &start, &end);
            char* text = gtk().textBufferGetText(buffer, &start, &end, true);
            ((Widget*)widget)->text = text;
            gtk().gFree(text);
        }

        bool settingSelection = false;
        void onSelected(void* dropDown, void*, void* data) {
            Click* click = (Click*)data;
            click->target->selected = gtk().dropDownGetSelected(dropDown);
            if(settingSelection) return;
            if(!(*click->onClick)(click->widget) && currentWindow) gtk().windowDestroy(currentWindow);
        }

        // typing makes the active index -1, picking an option makes it that option's index
        void onComboChanged(void* comboBox, void* data) {
            Click* click = (Click*)data;
            int active = gtk().comboBoxGetActive(comboBox);
            click->target->text = active >= 0? click->target->options[active]: gtk().editableGetText(gtk().comboBoxGetChild(comboBox));
            if(active >= 0 && !(*click->onClick)(click->widget) && currentWindow) gtk().windowDestroy(currentWindow);
        }


        // show

        string show(const Window& window, const ClickHandler& onClick, const ShowHandler& onShow = {}) {

            if(!gtk().gtk) return "The gui module requires GTK 4. Therefore, please install it.";
            if(!started()) return "Couldn't open a window, is there a display?";
            if(currentWindow) return "A window is already open.";

            bool closed = false;
            vector<Click> clicks(window.widgets.size());

            // add widgets
            void* grid = gtk().gridNew();
            gtk().gridSetRowSpacing   (grid, window.spacing);
            gtk().gridSetColumnSpacing(grid, window.spacing);
            for(size_t i = 0; i < window.widgets.size(); ++i) {
                Widget& widget = *window.widgets[i];

                switch(widget.type) {

                    case WIDGET_LABEL:
                        widget.handle = gtk().labelNew(widget.text.c_str());
                        gtk().gridAttach(grid, widget.handle, widget.x, widget.y, widget.xSpan, widget.ySpan);
                        break;

                    case WIDGET_BUTTON: {
                        void* button = gtk().buttonNewWithLabel(widget.text.c_str());
                        gtk().widgetSetHalign(button, 3);
                        gtk().widgetSetValign(button, 3);
                        clicks[i] = {i, &onClick};
                        gtk().signalConnectData(button, "clicked", (void(*)())onClicked, &clicks[i], nullptr, 0);
                        gtk().gridAttach(grid, button, widget.x, widget.y, widget.xSpan, widget.ySpan);
                        widget.handle = button;
                        break;
                    }

                    case WIDGET_TEXTBOX: {
                        void* entry = gtk().entryNew();
                        gtk().editableSetText(entry, widget.text.c_str());
                        gtk().widgetSetValign(entry, 3);
                        gtk().signalConnectData(entry, "changed", (void(*)())onChanged, &widget, nullptr, 0);
                        gtk().gridAttach(grid, entry, widget.x, widget.y, widget.xSpan, widget.ySpan);
                        widget.handle = entry;
                        break;
                    }

                    case WIDGET_TEXTAREA: {
                        void* view   = gtk().textViewNew();
                        void* buffer = gtk().textViewGetBuffer(view);
                        gtk().textViewSetWrapMode    (view, 3);    // GTK_WRAP_WORD_CHAR
                        gtk().textViewSetLeftMargin  (view, 8);    // padding, similar to a GtkEntry's
                        gtk().textViewSetRightMargin (view, 8);
                        gtk().textViewSetTopMargin   (view, 6);
                        gtk().textViewSetBottomMargin(view, 6);
                        gtk().textBufferSetText(buffer, widget.text.c_str(), -1);
                        gtk().signalConnectData(buffer, "changed", (void(*)())onBufferChanged, &widget, nullptr, 0);
                        void* scroll = gtk().scrolledWindowNew();
                        gtk().scrolledWindowSetChild   (scroll, view);
                        gtk().scrolledWindowSetHasFrame(scroll, true);
                        gtk().widgetSetSizeRequest     (scroll, 250, 100);
                        gtk().gridAttach(grid, scroll, widget.x, widget.y, widget.xSpan, widget.ySpan);
                        widget.handle = view;
                        break;
                    }

                    case WIDGET_SELECT: {
                        vector<const char*> strings;
                        for(const string& option: widget.options) strings.push_back(option.c_str());
                        strings.push_back(nullptr);
                        void* dropDown = gtk().dropDownNewFromStrings(strings.data());
                        gtk().dropDownSetSelected(dropDown, widget.selected);
                        gtk().widgetSetHalign(dropDown, 3);
                        gtk().widgetSetValign(dropDown, 3);
                        clicks[i] = {i, &onClick, &widget};
                        gtk().signalConnectData(dropDown, "notify::selected", (void(*)())onSelected, &clicks[i], nullptr, 0);
                        gtk().gridAttach(grid, dropDown, widget.x, widget.y, widget.xSpan, widget.ySpan);
                        widget.handle = dropDown;
                        break;
                    }

                    case WIDGET_COMBOBOX: {
                        // todo: GtkComboBoxText has been deprecated since 4.10
                        void* comboBox = gtk().comboBoxTextNewWithEntry();
                        for(const string& option: widget.options) gtk().comboBoxTextAppendText(comboBox, option.c_str());
                        gtk().editableSetText(gtk().comboBoxGetChild(comboBox), widget.text.c_str());
                        gtk().widgetSetValign(comboBox, 3);
                        clicks[i] = {i, &onClick, &widget};
                        gtk().signalConnectData(comboBox, "changed", (void(*)())onComboChanged, &clicks[i], nullptr, 0);
                        gtk().gridAttach(grid, comboBox, widget.x, widget.y, widget.xSpan, widget.ySpan);
                        widget.handle = comboBox;
                        break;
                    }

                }

            }
            gtk().widgetSetHalign(grid, 3);
            gtk().widgetSetValign(grid, 3);

            // make window
            void* gtkWindow = gtk().windowNew();
            gtk().windowSetTitle      (gtkWindow, window.title.c_str());
            gtk().windowSetDefaultSize(gtkWindow, window.width, window.height);
            gtk().windowSetChild      (gtkWindow, grid);
            gtk().signalConnectData   (gtkWindow, "destroy", (void(*)())onDestroy, &closed, nullptr, 0);
            gtk().windowPresent       (gtkWindow);
            currentWindow = gtkWindow;

            // block execution until the window gets closed
            if(onShow && !onShow() && currentWindow) gtk().windowDestroy(currentWindow);
            while(!closed) gtk().mainContextIteration(nullptr, true);

            // without this, the window might not close properly
            while(gtk().mainContextIteration(nullptr, false));

            for(Widget* widget: window.widgets) widget->handle = nullptr;
            return "";

        }

        void close() {
            if(currentWindow) gtk().windowDestroy(currentWindow);
        }

        void setText(Widget& widget, const string& text) {
            widget.text = text;
            if(!currentWindow || !widget.handle) return;
            switch(widget.type) {
                case WIDGET_LABEL:    gtk().labelSetText     (widget.handle, text.c_str());                              break;
                case WIDGET_BUTTON:   gtk().buttonSetLabel   (widget.handle, text.c_str());                              break;
                case WIDGET_TEXTBOX:  gtk().editableSetText  (widget.handle, text.c_str());                              break;
                case WIDGET_TEXTAREA: gtk().textBufferSetText(gtk().textViewGetBuffer(widget.handle), text.c_str(), -1); break;
                case WIDGET_SELECT:   break;    // uses setSelected instead
                case WIDGET_COMBOBOX: gtk().editableSetText  (gtk().comboBoxGetChild(widget.handle), text.c_str());      break;
            }
        }

        void setSelected(Widget& widget, int index) {
            widget.selected = index;
            if(!currentWindow || !widget.handle) return;
            settingSelection = true;
            gtk().dropDownSetSelected(widget.handle, index);
            settingSelection = false;
        }


	// WIN32
	
	#elifdef CARO_GUI_WIN32
	
        // constants
        const int firstId        = 100;
        const int buttonWidth    = 120;
        const int buttonHeight   = 28;
        const int buttonInset    = 24;
        const int textboxWidth   = 200;
        const int textboxHeight  = 24;
        const int textareaWidth  = 250;
        const int textareaHeight = 100;
        const int selectHeight   = 24;
        const int selectInset    = 40;

        // the window being shown
        HWND                currentWindow = nullptr;
        const Window*       currentLayout = nullptr;
        const ClickHandler* currentClick  = nullptr;

        // without this, you'd get the windows 3.1 font
        HFONT guiFont() {
            static HFONT font = []{
                NONCLIENTMETRICSW metrics = {sizeof(metrics)};
                if(SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)) {
                    return CreateFontIndirectW(&metrics.lfMessageFont);
                }
                return (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            }();
            return font;
        }

        // utf-8 to utf-16
        std::wstring wide(const string& text) {
            int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
            std::wstring result(size, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, result.data(), size);
            result.pop_back();
            return result;
        }

        // text of a control, utf-16 to utf-8
        string controlText(HWND control) {
            std::wstring text(GetWindowTextLengthW(control) + 1, L'\0');
            text.resize(GetWindowTextW(control, text.data(), (int)text.size()));
            int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0, nullptr, nullptr);
            string result(size, '\0');
            WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), result.data(), size, nullptr, nullptr);
            return result;
        }

        string fromCRLF(const string& text) {
            string result;
            for(char c: text) if(c != '\r') result += c;
            return result;
        }
        string toCRLF(const string& text) {
            string result;
            for(char c: fromCRLF(text)) {
                if(c == '\n') result += '\r';
                result += c;
            }
            return result;
        }

        SIZE measureText(HWND hwnd, const string& text, int maxWidth) {
            std::wstring wideText = wide(text);
            HDC     dc  = GetDC(hwnd);
            HGDIOBJ old = SelectObject(dc, guiFont());
            RECT    box = {0, 0, maxWidth, 0};
            DrawTextW(dc, wideText.c_str(), -1, &box, DT_CENTER | DT_WORDBREAK | DT_CALCRECT);
            SelectObject(dc, old);
            ReleaseDC(hwnd, dc);
            return {box.right, box.bottom};
        }

        int selectWidth(HWND hwnd, const Widget& widget, int maxWidth) {
            int width = 0;
            for(const string& option: widget.options) width = std::max(width, (int)measureText(hwnd, option, maxWidth).cx);
            return width + selectInset;
        }

        int trackStart(const vector<int>& tracks, int start) {
            int position = 0;
            for(int i = 0; i < start; ++i) position += tracks[i] + currentLayout->spacing;
            return position;
        }
        int trackSize(const vector<int>& tracks, int start, int span) {
            return std::max(trackStart(tracks, start + span) - trackStart(tracks, start) - currentLayout->spacing, 0);
        }

        struct Span{
            int start, span, size;
        };

        vector<int> fitTracks(const vector<Span>& spans) {
            int count = 0;
            for(const Span& s: spans) count = std::max(count, s.start + s.span);
            vector<int> tracks(count, 0);
            for(bool single: {true, false}) {
                for(const Span& s: spans) {
                    if((s.span == 1) != single) continue;
                    int missing = s.size - trackSize(tracks, s.start, s.span);
                    if(missing <= 0) continue;
                    for(int i = 0; i < s.span; ++i) tracks[s.start + i] += (missing + s.span - 1) / s.span;
                }
            }
            return tracks;
        }

        // callback
        LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {

            switch(message) {

                case WM_CREATE: {

                    for(size_t i = 0; i < currentLayout->widgets.size(); ++i) {

                        Widget& widget = *currentLayout->widgets[i];

                        HWND control;
                        
                        switch(widget.type) {
                            
                            case WIDGET_LABEL:
                                control = CreateWindowExW(
                                    0,
                                    L"STATIC",
                                    wide(widget.text).c_str(),
                                    WS_CHILD | WS_VISIBLE | SS_CENTER,
                                    0, 0, 0, 0,
                                    hwnd,
                                    (HMENU)(INT_PTR)(firstId + i),
                                    (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE),
                                    nullptr
                                );
                                break;

                            case WIDGET_BUTTON:
                                control = CreateWindowExW(
                                    0,
                                    L"BUTTON",
                                    wide(widget.text).c_str(),
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                    0, 0, 0, 0,
                                    hwnd,
                                    (HMENU)(INT_PTR)(firstId + i),
                                    (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE),
                                    nullptr
                                );
                                break;

                            case WIDGET_TEXTBOX:
                                control = CreateWindowExW(
                                    WS_EX_CLIENTEDGE,
                                    L"EDIT",
                                    wide(widget.text).c_str(),
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                    0, 0, 0, 0,
                                    hwnd,
                                    (HMENU)(INT_PTR)(firstId + i),
                                    (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE),
                                    nullptr
                                );
                                break;

                            case WIDGET_TEXTAREA:
                                control = CreateWindowExW(
                                    WS_EX_CLIENTEDGE,
                                    L"EDIT",
                                    wide(toCRLF(widget.text)).c_str(),
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                                    0, 0, 0, 0,
                                    hwnd,
                                    (HMENU)(INT_PTR)(firstId + i),
                                    (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE),
                                    nullptr
                                );
                                break;

                            case WIDGET_SELECT:
                                control = CreateWindowExW(
                                    0,
                                    L"COMBOBOX",
                                    L"",
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
                                    0, 0, 0, 0,
                                    hwnd,
                                    (HMENU)(INT_PTR)(firstId + i),
                                    (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE),
                                    nullptr
                                );
                                for(const string& option: widget.options) SendMessageW(control, CB_ADDSTRING, 0, (LPARAM)wide(option).c_str());
                                SendMessageW(control, CB_SETCURSEL, widget.selected, 0);
                                break;

                            case WIDGET_COMBOBOX:
                                control = CreateWindowExW(
                                    0,
                                    L"COMBOBOX",
                                    L"",
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWN | CBS_AUTOHSCROLL,
                                    0, 0, 0, 0,
                                    hwnd,
                                    (HMENU)(INT_PTR)(firstId + i),
                                    (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE),
                                    nullptr
                                );
                                for(const string& option: widget.options) SendMessageW(control, CB_ADDSTRING, 0, (LPARAM)wide(option).c_str());
                                SetWindowTextW(control, wide(widget.text).c_str());
                                break;

                        }

                        SendMessageW(control, WM_SETFONT, (WPARAM)guiFont(), true);
                        widget.handle = control;

                    }

                    return 0;

                }

                case WM_SIZE: {

                    int width  = LOWORD(lParam);
                    int height = HIWORD(lParam);
                    const vector<Widget*>& widgets = currentLayout->widgets;
                    int spacing = currentLayout->spacing;
                    int available = std::max(width - 2 * spacing, 0);

                    vector<Span> xSpans;
                    for(const Widget* widget: widgets) {
                        int size;
                        switch(widget->type) {
                            case WIDGET_LABEL:    size = measureText(hwnd, widget->text, available).cx;                                           break;
                            case WIDGET_BUTTON:   size = std::max(buttonWidth, (int)measureText(hwnd, widget->text, available).cx + buttonInset); break;
                            case WIDGET_TEXTBOX:  size = textboxWidth;                                                                            break;
                            case WIDGET_TEXTAREA: size = textareaWidth;                                                                           break;
                            case WIDGET_SELECT:   size = selectWidth(hwnd, *widget, available);                                                   break;
                            case WIDGET_COMBOBOX: size = std::max(textboxWidth, selectWidth(hwnd, *widget, available));                           break;
                        }
                        xSpans.push_back({widget->x, widget->xSpan, size});
                    }
                    vector<int> columns = fitTracks(xSpans);

                    int gaps    = spacing * std::max((int)columns.size() - 1, 0);
                    int content = trackSize(columns, 0, (int)columns.size()) - gaps;
                    if(content > available - gaps && content > 0) {
                        int target = std::max(available - gaps, 0);
                        for(int& column: columns) column = column * target / content;
                    }

                    vector<Span> ySpans;
                    vector<int>  heights;
                    for(const Widget* widget: widgets) {
                        int cellWidth = trackSize(columns, widget->x, widget->xSpan);
                        switch(widget->type) {
                            case WIDGET_LABEL:    heights.push_back(measureText(hwnd, widget->text, cellWidth).cy); break;
                            case WIDGET_BUTTON:   heights.push_back(buttonHeight);                                  break;
                            case WIDGET_TEXTBOX:  heights.push_back(textboxHeight);                                 break;
                            case WIDGET_TEXTAREA: heights.push_back(textareaHeight);                                break;
                            case WIDGET_SELECT:   heights.push_back(selectHeight);                                  break;
                            case WIDGET_COMBOBOX: heights.push_back(selectHeight);                                  break;
                        }
                        ySpans.push_back({widget->y, widget->ySpan, heights.back()});
                    }
                    vector<int> rows = fitTracks(ySpans);

                    int left = (width  - trackSize(columns, 0, (int)columns.size())) / 2;
                    int top  = (height - trackSize(rows,    0, (int)rows.size()   )) / 2;
                    for(size_t i = 0; i < widgets.size(); ++i) {
                        const Widget& widget = *widgets[i];
                        int cellX      = left + trackStart(columns, widget.x);
                        int cellY      = top  + trackStart(rows,    widget.y);
                        int cellWidth  = trackSize(columns, widget.x, widget.xSpan);
                        int cellHeight = trackSize(rows,    widget.y, widget.ySpan);
                        HWND control = GetDlgItem(hwnd, firstId + i);
                        if(widget.type == WIDGET_BUTTON) {
                            int buttonW = std::min(xSpans[i].size, cellWidth);
                            MoveWindow(control, cellX + (cellWidth - buttonW) / 2, cellY + (cellHeight - buttonHeight) / 2, buttonW, buttonHeight, true);
                        } else if(widget.type == WIDGET_SELECT || widget.type == WIDGET_COMBOBOX) {
                            int selectW    = widget.type == WIDGET_SELECT? std::min(xSpans[i].size, cellWidth): cellWidth;
                            int listHeight = (int)std::min<size_t>(widget.options.size(), 8) * (int)SendMessageW(control, CB_GETITEMHEIGHT, 0, 0) + 2;    // the dropdown is part of the control's height
                            MoveWindow(control, cellX + (cellWidth - selectW) / 2, cellY + (cellHeight - selectHeight) / 2, selectW, selectHeight + listHeight, true);
                        } else {
                            MoveWindow(control, cellX, cellY + (cellHeight - heights[i]) / 2, cellWidth, heights[i], true);
                        }
                    }

                    return 0;

                }

                case WM_COMMAND: {
                    int id = LOWORD(wParam) - firstId;
                    if(id < 0 || id >= (int)currentLayout->widgets.size()) break;
                    Widget& widget = *currentLayout->widgets[id];
                    if(widget.type == WIDGET_BUTTON) {
                        if(!(*currentClick)(id) && currentWindow) DestroyWindow(currentWindow);
                        return 0;
                    }
                    if((widget.type == WIDGET_TEXTBOX || widget.type == WIDGET_TEXTAREA) && HIWORD(wParam) == EN_CHANGE) {
                        widget.text = fromCRLF(controlText((HWND)lParam));
                        return 0;
                    }
                    if(widget.type == WIDGET_SELECT && HIWORD(wParam) == CBN_SELCHANGE) {
                        widget.selected = (int)SendMessageW((HWND)lParam, CB_GETCURSEL, 0, 0);
                        if(!(*currentClick)(id) && currentWindow) DestroyWindow(currentWindow);
                        return 0;
                    }
                    if(widget.type == WIDGET_COMBOBOX && HIWORD(wParam) == CBN_EDITCHANGE) {
                        widget.text = controlText((HWND)lParam);
                        return 0;
                    }
                    if(widget.type == WIDGET_COMBOBOX && HIWORD(wParam) == CBN_SELCHANGE) {
                        int index = (int)SendMessageW((HWND)lParam, CB_GETCURSEL, 0, 0);
                        if(index < 0) return 0;
                        widget.text = widget.options[index];
                        if(!(*currentClick)(id) && currentWindow) DestroyWindow(currentWindow);
                        return 0;
                    }
                    break;
                }

                case WM_DESTROY:
                    currentWindow = nullptr;
                    PostQuitMessage(0);
                    return 0;

            }

            return DefWindowProcW(hwnd, message, wParam, lParam);

        }
        
		string show(const Window& window, const ClickHandler& onClick, const ShowHandler& onShow = {}) {

            if(currentWindow) return "A window is already open.";

            // make class
            const wchar_t CLASS_NAME[]  = L"Carotene window class";
            HINSTANCE hInstance = GetModuleHandleW(nullptr);
            WNDCLASSW wc = {};
            wc.lpfnWndProc   = windowProc;
            wc.hInstance     = hInstance;
            wc.lpszClassName = CLASS_NAME;
            wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
            wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);    // without this the client area is never erased

            // register class
            static bool registered = false;
            if(!registered) {
                if(!RegisterClassW(&wc)) return "Couldn't register the window class.";
                registered = true;
            }

            currentLayout = &window;
            currentClick  = &onClick;

            // make window
            HWND hwnd = CreateWindowExW(

                0,                                        // Optional window styles.
                CLASS_NAME,                               // Window class
                wide(window.title).c_str(),               // Window text
                WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,    // Window style

                CW_USEDEFAULT, CW_USEDEFAULT, window.width, window.height,    // Size and position

                nullptr,      // Parent window    
                nullptr,      // Menu
                hInstance,    // Instance handle
                nullptr       // Additional application data
                
            );
            if(hwnd == NULL) return "Couldn't make window.";
            currentWindow = hwnd;

            // show window
            ShowWindow(hwnd, SW_SHOWNORMAL);

            // block execution
            if(onShow && !onShow() && currentWindow) DestroyWindow(currentWindow);
            MSG message;
            while(GetMessageW(&message, nullptr, 0, 0) > 0) {
                if(IsDialogMessageW(hwnd, &message)) continue;    // so buttons can be pressed with the keyboard too
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }

            for(Widget* widget: window.widgets) widget->handle = nullptr;
            return "";

            // based on https://learn.microsoft.com/en-us/windows/win32/learnwin32/creating-a-window
            
        }
		
        void close() {
            if(currentWindow) DestroyWindow(currentWindow);
        }

        void setText(Widget& widget, const string& text) {

            widget.text = text;
            if(!currentWindow || !widget.handle) return;
            SetWindowTextW((HWND)widget.handle, wide(widget.type == WIDGET_TEXTAREA? toCRLF(text): text).c_str());

            RECT client;
            GetClientRect(currentWindow, &client);
            SendMessageW(currentWindow, WM_SIZE, SIZE_RESTORED, MAKELPARAM(client.right, client.bottom));
        }

        void setSelected(Widget& widget, int index) {
            widget.selected = index;
            if(!currentWindow || !widget.handle) return;
            SendMessageW((HWND)widget.handle, CB_SETCURSEL, index, 0);
        }

	
	// BEAPI
	
	#elifdef CARO_GUI_BEAPI
	
        const ClickHandler* currentClick = nullptr;
        const ShowHandler*  currentShow  = nullptr;

        // BTextView has no modification message, so this keeps the widget's text in sync itself
        class CaroTextView: public BTextView{

            public:

                CaroTextView(Widget& widget): BTextView("textarea") {
                    SetInsets(4, 4, 4, 4);
                    SetText(widget.text.c_str());
                    this->widget = &widget;
                }

            protected:

                void InsertText(const char* text, int32 length, int32 offset, const text_run_array* runs) override{
                    BTextView::InsertText(text, length, offset, runs);
                    if(widget) widget->text = Text();
                }

                void DeleteText(int32 start, int32 end) override{
                    BTextView::DeleteText(start, end);
                    if(widget) widget->text = Text();
                }

            private:

                Widget* widget = nullptr;

        };

		class CaroWindow: public BWindow{
	
			public:
			
				CaroWindow(const Window& window):

                    // make window
					BWindow(
						BRect(0, 0, window.width, window.height),    // gets centered later
						window.title.c_str(),
						B_TITLED_WINDOW,
						B_ASYNCHRONOUS_CONTROLS | B_QUIT_ON_WINDOW_CLOSE | B_AUTO_UPDATE_SIZE_LIMITS
					)

                    // add widgets
					{

                        // start building
						BLayoutBuilder::Group<> builder(this, B_VERTICAL, B_USE_DEFAULT_SPACING);
						builder
                            .SetInsets(B_USE_WINDOW_INSETS)    // set insets
                            .AddGlue();                        // top glue

                        // grid
						BGridLayout* grid = new BGridLayout(window.spacing, window.spacing);
						builder.AddGroup(B_HORIZONTAL)
							        .AddGlue()    // left glue
							        .Add(grid)    // content
							        .AddGlue()    // right glue
                                .End()
                                .AddGlue();       // bottom glue

                        // widgets
						for(size_t i = 0; i < window.widgets.size(); ++i) {

							Widget& widget = *window.widgets[i];
							BView* view;

							switch(widget.type) {

                                case WIDGET_LABEL:
                                    view = new BStringView("label", widget.text.c_str());
                                    break;
                                
                                case WIDGET_BUTTON: {
                                    BMessage* message = new BMessage('clik');
                                    message->AddInt32("widget", (int32)i);
                                    view = new BButton("button", widget.text.c_str(), message);
                                    break;
                                }

                                case WIDGET_TEXTBOX: {
                                    BTextControl* textbox = new BTextControl("textbox", nullptr, widget.text.c_str(), nullptr);
                                    BMessage* message = new BMessage('chtx');
                                    message->AddPointer("widget", &widget);
                                    textbox->SetModificationMessage(message);
                                    view = textbox;
                                    break;
                                }

                                case WIDGET_TEXTAREA: {
                                    BScrollView* scroll = new BScrollView("Textarea scroll", new CaroTextView(widget), 0, false, true);
                                    scroll->SetExplicitMinSize(BSize(250, 100));
                                    view = scroll;
                                    break;
                                }

                                // beapi doesn't have a combobox and the "workaround" doesn't really work so just make it a select
                                case WIDGET_SELECT: case WIDGET_COMBOBOX: {
                                    if(widget.type == WIDGET_COMBOBOX) {
                                        auto found = std::ranges::find(widget.options, widget.text);
                                        widget.selected = found == widget.options.end()? 0: (int)(found - widget.options.begin());
                                        widget.text = widget.options[widget.selected];
                                    }
                                    BPopUpMenu* menu = new BPopUpMenu("select");
                                    for(size_t j = 0; j < widget.options.size(); ++j) {
                                        BMessage* message = new BMessage('slct');
                                        message->AddPointer("widget", &widget);
                                        message->AddInt32("index",  (int32)i);
                                        message->AddInt32("option", (int32)j);
                                        menu->AddItem(new BMenuItem(widget.options[j].c_str(), message));
                                    }
                                    if(BMenuItem* item = menu->ItemAt(widget.selected)) item->SetMarked(true);
                                    view = new BMenuField("select", nullptr, menu);
                                    break;
                                }

							}

                            // fails if the cells overlap another widget
							BLayoutItem* item = grid->AddView(view, widget.x, widget.y, widget.xSpan, widget.ySpan);
							if(item == nullptr) {
								delete view;
								continue;
							}
							item->SetExplicitAlignment(BAlignment(
                                widget.type == WIDGET_TEXTBOX || widget.type == WIDGET_TEXTAREA? B_ALIGN_USE_FULL_WIDTH:  B_ALIGN_HORIZONTAL_CENTER,
                                widget.type == WIDGET_TEXTAREA?                                  B_ALIGN_USE_FULL_HEIGHT: B_ALIGN_VERTICAL_CENTER
                            ));
                            widget.handle = view;

						}

                        // layout
						Layout(true);
						CenterOnScreen();

					}
				
				void MessageReceived(BMessage* message) override{
					switch (message->what) {

						case 'chtx': {
							Widget* widget = nullptr;
							if(message->FindPointer("widget", (void**)&widget) == B_OK && widget->handle) {
								widget->text = static_cast<BTextControl*>(widget->handle)->Text();
							}
							break;
						}

						case 'slct': {
							Widget* widget = nullptr;
							if(message->FindPointer("widget", (void**)&widget) != B_OK) break;
							widget->selected = message->GetInt32("option", 0);
							if(widget->type == WIDGET_COMBOBOX) widget->text = widget->options[widget->selected];
							if(!(*currentClick)(message->GetInt32("index", 0))) be_app->PostMessage(B_QUIT_REQUESTED);
							break;
						}

						case 'clik':
							if(!(*currentClick)(message->GetInt32("widget", 0))) be_app->PostMessage(B_QUIT_REQUESTED);
							break;

						case 'show':
							if(*currentShow && !(*currentShow)()) be_app->PostMessage(B_QUIT_REQUESTED);
							break;

						default:
							BWindow::MessageReceived(message);

					}
				}
				
		};

		string show(const Window& window, const ClickHandler& onClick, const ShowHandler& onShow = {}) {
            if(be_app) return "A window is already open.";
			BApplication app("application/x-vnd.Carotene-Window");
            currentClick = &onClick;
            currentShow  = &onShow;
			CaroWindow* caroWindow = new CaroWindow(window);
			caroWindow->Show();
			if(onShow) caroWindow->PostMessage('show');
			app.Run();
            for(Widget* widget: window.widgets) widget->handle = nullptr;
			return "";
		}

        void close() {
            if(be_app) be_app->PostMessage(B_QUIT_REQUESTED);
        }

        void setText(Widget& widget, const string& text) {
            widget.text = text;
            BView* view = (BView*)widget.handle;
            if(view == nullptr || !view->LockLooper()) return;
            switch(widget.type) {
                case WIDGET_LABEL:    static_cast<BStringView*> (view)->SetText (text.c_str());                                  break;
                case WIDGET_BUTTON:   static_cast<BButton*>     (view)->SetLabel(text.c_str());                                  break;
                case WIDGET_TEXTBOX:  static_cast<BTextControl*>(view)->SetText (text.c_str());                                  break;
                case WIDGET_TEXTAREA: static_cast<BTextView*>(static_cast<BScrollView*>(view)->Target())->SetText(text.c_str()); break;
                case WIDGET_SELECT:                                                                                              break;
                case WIDGET_COMBOBOX: {
                    BMenu* menu = static_cast<BMenuField*>(view)->Menu();
                    auto found = std::ranges::find(widget.options, text);
                    if(found != widget.options.end()) {
                        widget.selected = (int)(found - widget.options.begin());
                        if(BMenuItem* item = menu->ItemAt(widget.selected)) item->SetMarked(true);
                    } else {
                        if(BMenuItem* marked = menu->FindMarked()) marked->SetMarked(false);
                        if(BMenuItem* super = menu->Superitem()) super->SetLabel(text.c_str());
                    }
                    break;
                }
            }
            view->UnlockLooper();
        }

        void setSelected(Widget& widget, int index) {
            widget.selected = index;
            BView* view = (BView*)widget.handle;
            if(view == nullptr || !view->LockLooper()) return;
            if(BMenuItem* item = static_cast<BMenuField*>(view)->Menu()->ItemAt(index)) item->SetMarked(true);
            view->UnlockLooper();
        }

        // the haiku convention is to use 4 char chars for ints, which seems kinda funny but we'll follow it

        
    #endif


}
