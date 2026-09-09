
#pragma once

/*

    3 backends:
      - Windows:               Win32, linked normally
      - macOS, Linux, FreeBSD: GTK 4, with dlopen()
      - Haiku:                 BeAPI, linked normally
    
    Maybe Cocoa and Qt later.

*/


// INCLUDES

#if defined(_WIN32)
	#define CARO_GUI_WIN32
	
	// tba
	
#elif defined(__HAIKU__)
	#define CARO_GUI_BEAPI
		
	#include <Application.h>
	#include <Window.h>
	#include <StringView.h>
	#include <Button.h>
	#include <LayoutBuilder.h>

#else
    #define CARO_GUI_GTK
	
    #include <dlfcn.h>
	
#endif

#include "../../core/format.hpp"


namespace CaroGui{


    // BACKENDS


    // gtk

    #ifdef CARO_GUI_GTK


        // gtk functions

        #define gtkFunctions(f)\
            \
            /* GTK */\
            f(initCheck,            "gtk_init_check",              int,           (void))\
            f(windowNew,            "gtk_window_new",              void*,         (void))\
            f(windowSetTitle,       "gtk_window_set_title",        void,          (void*, const char*))\
            f(windowSetDefaultSize, "gtk_window_set_default_size", void,          (void*, int, int))\
            f(windowSetChild,       "gtk_window_set_child",        void,          (void*, void*))\
            f(windowPresent,        "gtk_window_present",          void,          (void*))\
            f(windowDestroy,        "gtk_window_destroy",          void,          (void*))\
            f(labelNew,             "gtk_label_new",               void*,         (const char*))\
            f(buttonNewWithLabel,   "gtk_button_new_with_label",   void*,         (const char*))\
            f(boxNew,               "gtk_box_new",                 void*,         (int, int))\
            f(boxAppend,            "gtk_box_append",              void,          (void*, void*))\
            f(widgetSetHalign,      "gtk_widget_set_halign",       void,          (void*, int))\
            f(widgetSetValign,      "gtk_widget_set_valign",       void,          (void*, int))\
            \
            /* GObject */\
            f(signalConnectData,    "g_signal_connect_data",       unsigned long, (void*, const char*, void(*)(), void*, void*, int))\
            \
            /* GLib */\
            f(mainContextIteration, "g_main_context_iteration",    int,           (void*, int))


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

        void onDestroy(void*, void* closed)  { *(bool*)closed = true;     }
        void onClicked(void*, void* window)  { gtk().windowDestroy(window); }


        // test

        string test() {

            if(!gtk().gtk) return "The gui module requires GTK 4. Therefore, please install it.";
            if(!started()) return "Couldn't open a window, is there a display?";

            bool closed = false;

            // add button
            void* button = gtk().buttonNewWithLabel("Self-destruct");
            gtk().widgetSetHalign(button, 3);

            // add box and label
            void* box = gtk().boxNew(1, 25);
            gtk().boxAppend      (box, gtk().labelNew("Hello! This is a test window rendered using GTK 4. Pretty cool!"));
            gtk().boxAppend      (box, button);
            gtk().widgetSetHalign(box, 3);
            gtk().widgetSetValign(box, 3);

            // make window
            void* window = gtk().windowNew();
            gtk().windowSetTitle      (window, "Carotene test window");
            gtk().windowSetDefaultSize(window, 400, 250);
            gtk().windowSetChild      (window, box);
            gtk().signalConnectData   (window, "destroy", (void(*)())onDestroy, &closed, nullptr, 0);
            gtk().signalConnectData   (button, "clicked", (void(*)())onClicked, window,  nullptr, 0);
            gtk().windowPresent       (window);

            // block execution until the window gets closed
            while(!closed) gtk().mainContextIteration(nullptr, true);

            // without this, the window might not close properly
            while(gtk().mainContextIteration(nullptr, false));

            return "";

        }


	// win32
	
	#elifdef CARO_GUI_WIN32
	
		string test() {
            return "The gui module doesn't support Win32 yet.";
        }
		
	
	// beapi
	
	#elifdef CARO_GUI_BEAPI
	
		class CaroWindow: public BWindow{
	
			public:
			
				CaroWindow():
					BWindow(
						BRect(0, 0, 400, 250),    // gets centered later
						"Carotene test window",
						B_TITLED_WINDOW,
						B_ASYNCHRONOUS_CONTROLS | B_QUIT_ON_WINDOW_CLOSE | B_AUTO_UPDATE_SIZE_LIMITS
					)
					{
						BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_DEFAULT_SPACING)
							.SetInsets(B_USE_WINDOW_INSETS)
							.AddGlue()
							.AddGroup(B_HORIZONTAL)
								.AddGlue()
								.Add(new BStringView("label", "Hello! This is a test window rendered using BeAPI. Pretty cool!"))
								.AddGlue()
							.End()
							.AddGroup(B_HORIZONTAL)
								.AddGlue()
								.Add(new BButton("button", "Self-destruct", new BMessage('sfdt')))
								.AddGlue()
							.End()
							.AddGlue();
								// the glues make everything centered
						Layout(true);
						CenterOnScreen();
					}
				
				void MessageReceived(BMessage* message) override{
					switch (message->what) {
						case 'sfdt':
							be_app->PostMessage(B_QUIT_REQUESTED);
							break;
						default:
							BWindow::MessageReceived(message);
					}
				}
				
		};

		string test() {
			BApplication app("application/x-vnd.Carotene-WindowTest");
			(new CaroWindow())->Show();
			app.Run();
			return "";
		}
		
    #endif


}
