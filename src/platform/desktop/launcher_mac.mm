// The macOS launcher window: file selection, settings and launching the game.

#import <Cocoa/Cocoa.h>

#include <spawn.h>
#include <sys/wait.h>

#include <string>
#include <vector>

#include "platform/desktop/game_app.h"
#include "platform/desktop/library.h"

extern char** environ;

namespace {

constexpr CGFloat kContentWidth = 440;

NSDictionary<NSString*, NSDictionary<NSString*, NSString*>*>* Texts() {
  static NSDictionary* texts = @{
    @"en" : @{
      @"instructions" : @"Instructions",
      @"loadOriginals" : @"Load the original files from your own copy of the game:",
      @"chdUse" : @"19990930.chd: BGAs, Steps and Sprites.",
      @"romUse" : @"piu10.u9: Additional sound effects (Grades, Coins, Menus, etc.).",
      @"fileSelection" : @"File selection",
      @"addSounds" : @"Add sound effects (piu10.u9)",
      @"changeSounds" : @"Change piu10.u9 file",
      @"extractNote" : @"Files are extracted only once and a path will be generated inside the game folder.",
      @"legalNote" : @"The files used by this engine must be obtained through a legal backup of your own copy of the game.",
      @"additionalSettings" : @"Additional settings",
      @"aspectRatio" : @"Aspect ratio",
      @"aspectOriginal" : @"Original (4:3)",
      @"aspectWide" : @"Widescreen (16:9)",
      @"menuLanguage" : @"Menu language",
      @"languageEnglish" : @"English",
      @"languageSpanish" : @"Spanish",
      @"displayMode" : @"Display mode",
      @"displayWindowed" : @"Windowed",
      @"displayFullscreen" : @"Fullscreen",
      @"mapControls" : @"Map controls",
      @"launch" : @"Launch game",
      @"chooseChd" : @"Choose CHD file",
      @"changeChd" : @"Change CHD file",
      @"noGame" : @"No game loaded yet.",
      @"ready" : @"Ready: %@",
      @"soundsReady" : @"Sound effects ready.",
      @"noSounds" : @"No sound effects (optional).",
      @"opening" : @"Opening the CHD…",
      @"extracting" : @"Extracting… %@%%",
      @"readingSounds" : @"Reading the sound effects…",
      @"no-space" : @"There is not enough space on this disk.",
      @"not-chd" : @"That file could not be opened as a CHD.",
      @"not-cd" : @"That CHD is not a CD image.",
      @"wrong-disc" : @"That CHD is not this game's disc.",
      @"damaged" : @"The game's files are damaged on that CHD.",
      @"read-failed" : @"Part of that CHD could not be read.",
      @"wrong-sound-rom" : @"That file is not the game's sound ROM (piu10.u9).",
    },
    @"es" : @{
      @"instructions" : @"Instrucciones",
      @"loadOriginals" : @"Carga los archivos originales de tu propia copia del juego:",
      @"chdUse" : @"19990930.chd: BGAs, Steps y Sprites.",
      @"romUse" : @"piu10.u9: Efectos de sonido adicionales (Grades, Monedas, Menús, etc.).",
      @"fileSelection" : @"Selección de archivos",
      @"addSounds" : @"Agregar efectos de sonido (piu10.u9)",
      @"changeSounds" : @"Cambiar archivo piu10.u9",
      @"extractNote" : @"Los archivos se extraen una sola vez y se generará un path dentro de la carpeta del juego.",
      @"legalNote" : @"Los archivos utilizados para este motor, deben ser obtenidos a través de un backup legal de tu propia copia del juego.",
      @"additionalSettings" : @"Configuraciones adicionales",
      @"aspectRatio" : @"Relación de aspecto",
      @"aspectOriginal" : @"Original (4:3)",
      @"aspectWide" : @"Panorámica (16:9)",
      @"menuLanguage" : @"Idioma del menú",
      @"languageEnglish" : @"Inglés",
      @"languageSpanish" : @"Español",
      @"displayMode" : @"Modo de pantalla",
      @"displayWindowed" : @"Ventana",
      @"displayFullscreen" : @"Pantalla completa",
      @"mapControls" : @"Mapear controles",
      @"launch" : @"Lanzar juego",
      @"chooseChd" : @"Elegir archivo CHD",
      @"changeChd" : @"Cambiar archivo CHD",
      @"noGame" : @"Todavía no hay juego cargado.",
      @"ready" : @"Listo: %@",
      @"soundsReady" : @"Efectos de sonido listos.",
      @"noSounds" : @"Sin efectos de sonido (opcional).",
      @"opening" : @"Abriendo el CHD…",
      @"extracting" : @"Extrayendo… %@%%",
      @"readingSounds" : @"Leyendo los efectos de sonido…",
      @"no-space" : @"No hay espacio suficiente en este disco.",
      @"not-chd" : @"No pude abrir ese archivo como CHD.",
      @"not-cd" : @"Ese CHD no es de un CD.",
      @"wrong-disc" : @"Ese CHD no es el disco de este juego.",
      @"damaged" : @"Los archivos del juego vienen dañados en ese CHD.",
      @"read-failed" : @"No pude leer una parte de ese CHD.",
      @"wrong-sound-rom" : @"Ese archivo no es la ROM de sonidos del juego (piu10.u9).",
    },
  };
  return texts;
}

NSColor* Sky() { return [NSColor colorWithSRGBRed:0x87 / 255.0 green:0xce / 255.0 blue:0xeb / 255.0 alpha:1.0]; }
NSColor* Warning() { return [NSColor colorWithSRGBRed:1.0 green:0x8a / 255.0 blue:0x5c / 255.0 alpha:1.0]; }

}  // namespace

@interface PiuLauncher : NSObject <NSApplicationDelegate>
@end

@implementation PiuLauncher {
  NSWindow* _window;
  NSStackView* _stack;
  NSMutableArray<NSTextField*>* _translated;
  NSTextField* _status;
  NSTextField* _soundStatus;
  NSProgressIndicator* _progress;
  NSButton* _chooseChd;
  NSButton* _addSounds;
  NSButton* _launch;
  NSButton* _mapKeys;
  NSPopUpButton* _aspect;
  NSPopUpButton* _language;
  NSPopUpButton* _display;
  piu::desktop::Settings _settings;
  piu::desktop::LibraryState _state;
  BOOL _busy;
  NSString* _statusKey;
  NSString* _statusArgument;
  BOOL _statusWarn;
}

- (NSString*)text:(NSString*)key {
  NSString* language = [NSString stringWithUTF8String:_settings.language.c_str()];
  return Texts()[language][key] ?: key;
}

- (NSTextField*)labelForKey:(NSString*)key size:(CGFloat)size bold:(BOOL)bold {
  NSTextField* label = [NSTextField wrappingLabelWithString:@""];
  label.selectable = NO;
  label.font = bold ? [NSFont boldSystemFontOfSize:size] : [NSFont systemFontOfSize:size];
  label.preferredMaxLayoutWidth = kContentWidth;
  [label.widthAnchor constraintEqualToConstant:kContentWidth].active = YES;
  if (key != nil) {
    label.identifier = key;
    [_translated addObject:label];
  }
  [_stack addArrangedSubview:label];
  return label;
}

- (void)addSeparator {
  NSBox* line = [[NSBox alloc] initWithFrame:NSZeroRect];
  line.boxType = NSBoxSeparator;
  [_stack addArrangedSubview:line];
  [line.widthAnchor constraintEqualToConstant:kContentWidth].active = YES;
  [line.heightAnchor constraintEqualToConstant:1].active = YES;
}

- (NSButton*)addButtonWithAction:(SEL)action {
  NSButton* button = [NSButton buttonWithTitle:@"" target:self action:action];
  [_stack addArrangedSubview:button];
  [button.widthAnchor constraintEqualToConstant:kContentWidth].active = YES;
  return button;
}

- (NSPopUpButton*)addPopupForKey:(NSString*)key action:(SEL)action {
  NSTextField* label = [self labelForKey:key size:12 bold:NO];
  label.textColor = NSColor.secondaryLabelColor;
  NSPopUpButton* popup = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
  popup.target = self;
  popup.action = action;
  [_stack addArrangedSubview:popup];
  [popup.widthAnchor constraintEqualToConstant:kContentWidth].active = YES;
  return popup;
}

- (void)fillPopup:(NSPopUpButton*)popup keys:(NSArray<NSString*>*)keys selected:(NSInteger)selected {
  [popup removeAllItems];
  for (NSString* key in keys) [popup addItemWithTitle:[self text:key]];
  [popup selectItemAtIndex:selected];
}

- (void)buildWindow {
  _translated = [NSMutableArray array];
  _stack = [[NSStackView alloc] initWithFrame:NSZeroRect];
  _stack.orientation = NSUserInterfaceLayoutOrientationVertical;
  _stack.alignment = NSLayoutAttributeLeading;
  _stack.spacing = 8;
  _stack.edgeInsets = NSEdgeInsetsMake(20, 20, 20, 20);
  _stack.translatesAutoresizingMaskIntoConstraints = NO;

  NSTextField* title = [self labelForKey:nil size:24 bold:YES];
  NSMutableAttributedString* name = [[NSMutableAttributedString alloc] initWithString:@"Open It UP: 1st Preservation"];
  [name addAttribute:NSFontAttributeName value:[NSFont boldSystemFontOfSize:24] range:NSMakeRange(0, name.length)];
  [name addAttribute:NSForegroundColorAttributeName value:NSColor.labelColor range:NSMakeRange(0, name.length)];
  [name addAttribute:NSForegroundColorAttributeName value:Sky() range:NSMakeRange(5, 5)];
  title.attributedStringValue = name;

  [self addSeparator];
  [self labelForKey:@"instructions" size:13 bold:YES];
  [self labelForKey:@"loadOriginals" size:13 bold:NO];
  [self labelForKey:@"chdUse" size:13 bold:NO];
  [self labelForKey:@"romUse" size:13 bold:NO];

  [self addSeparator];
  [self labelForKey:@"fileSelection" size:13 bold:YES];
  _status = [self labelForKey:nil size:13 bold:NO];
  _progress = [[NSProgressIndicator alloc] initWithFrame:NSZeroRect];
  _progress.style = NSProgressIndicatorStyleBar;
  _progress.indeterminate = NO;
  _progress.minValue = 0;
  _progress.maxValue = 100;
  _progress.hidden = YES;
  [_stack addArrangedSubview:_progress];
  [_progress.widthAnchor constraintEqualToConstant:kContentWidth].active = YES;
  _chooseChd = [self addButtonWithAction:@selector(chooseChd:)];
  _addSounds = [self addButtonWithAction:@selector(addSounds:)];
  _soundStatus = [self labelForKey:nil size:12 bold:NO];
  _soundStatus.textColor = NSColor.secondaryLabelColor;
  [self labelForKey:@"extractNote" size:12 bold:NO].textColor = NSColor.secondaryLabelColor;
  [self labelForKey:@"legalNote" size:12 bold:NO].textColor = NSColor.secondaryLabelColor;

  [self addSeparator];
  [self labelForKey:@"additionalSettings" size:13 bold:YES];
  _aspect = [self addPopupForKey:@"aspectRatio" action:@selector(settingChanged:)];
  _language = [self addPopupForKey:@"menuLanguage" action:@selector(languageChanged:)];
  _display = [self addPopupForKey:@"displayMode" action:@selector(settingChanged:)];
  _mapKeys = [self addButtonWithAction:@selector(mapKeys:)];

  [self addSeparator];
  _launch = [self addButtonWithAction:@selector(launch:)];
  _launch.keyEquivalent = @"\r";

  _window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, kContentWidth + 40, 600)
                                        styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable
                                          backing:NSBackingStoreBuffered
                                            defer:NO];
  _window.title = @"Open It UP: 1st Preservation";
  _window.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
  _window.releasedWhenClosed = NO;
  NSView* content = _window.contentView;
  [content addSubview:_stack];
  [NSLayoutConstraint activateConstraints:@[
    [_stack.topAnchor constraintEqualToAnchor:content.topAnchor],
    [_stack.bottomAnchor constraintEqualToAnchor:content.bottomAnchor],
    [_stack.leadingAnchor constraintEqualToAnchor:content.leadingAnchor],
    [_stack.trailingAnchor constraintEqualToAnchor:content.trailingAnchor],
  ]];
}

- (void)renderStatus {
  NSString* text = @"";
  if (_statusKey != nil) {
    text = [self text:_statusKey];
    if (_statusArgument != nil) text = [NSString stringWithFormat:text, _statusArgument];
  }
  _status.stringValue = text;
  _status.textColor = _statusWarn ? Warning() : NSColor.secondaryLabelColor;
}

- (void)setStatus:(NSString*)key argument:(NSString*)argument warn:(BOOL)warn {
  _statusKey = key;
  _statusArgument = argument;
  _statusWarn = warn;
  [self renderStatus];
}

- (void)renderControls {
  _chooseChd.title = [self text:_state.ready ? @"changeChd" : @"chooseChd"];
  _addSounds.title = [self text:_state.sounds_ready ? @"changeSounds" : @"addSounds"];
  _launch.title = [self text:@"launch"];
  _mapKeys.title = [self text:@"mapControls"];
  _mapKeys.enabled = !_busy;
  _soundStatus.stringValue = [self text:_state.sounds_ready ? @"soundsReady" : @"noSounds"];
  _chooseChd.enabled = !_busy;
  _addSounds.enabled = !_busy && _state.ready;
  _launch.enabled = !_busy && _state.ready;
  _progress.hidden = !_busy;
}

- (void)renderTexts {
  for (NSTextField* label in _translated) label.stringValue = [self text:label.identifier];
  [self fillPopup:_aspect keys:@[ @"aspectOriginal", @"aspectWide" ] selected:_settings.wide ? 1 : 0];
  [self fillPopup:_language keys:@[ @"languageEnglish", @"languageSpanish" ] selected:_settings.language == "es" ? 1 : 0];
  [self fillPopup:_display keys:@[ @"displayWindowed", @"displayFullscreen" ] selected:_settings.fullscreen ? 1 : 0];
  [self renderControls];
  [self renderStatus];
}

// The window follows its content, keeping its top edge in place.
- (void)fitWindow {
  [_stack layoutSubtreeIfNeeded];
  NSSize size = _stack.fittingSize;
  NSRect now = _window.frame;
  NSRect next = [_window frameRectForContentRect:NSMakeRect(0, 0, size.width, size.height)];
  next.origin.x = now.origin.x;
  next.origin.y = NSMaxY(now) - next.size.height;
  [_window setFrame:next display:YES];
}

- (void)showLibrary {
  _state = piu::desktop::read_library_state();
  if (_state.ready) {
    [self setStatus:@"ready" argument:[NSString stringWithUTF8String:_state.source.c_str()] warn:NO];
  } else {
    [self setStatus:@"noGame" argument:nil warn:NO];
  }
  [self renderControls];
  [self fitWindow];
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
  _settings = piu::desktop::load_settings();

  NSMenu* bar = [[NSMenu alloc] init];
  NSMenuItem* appItem = [[NSMenuItem alloc] init];
  [bar addItem:appItem];
  NSMenu* appMenu = [[NSMenu alloc] init];
  [appMenu addItemWithTitle:@"Quit Open It UP" action:@selector(terminate:) keyEquivalent:@"q"];
  appItem.submenu = appMenu;
  NSApp.mainMenu = bar;

  [self buildWindow];
  [self renderTexts];
  [self showLibrary];
  [_window center];
  [_window makeKeyAndOrderFront:nil];
  [NSApp activateIgnoringOtherApps:YES];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
  return YES;
}

- (void)storeChoices {
  _settings.wide = _aspect.indexOfSelectedItem == 1;
  _settings.fullscreen = _display.indexOfSelectedItem == 1;
  piu::desktop::save_settings(_settings);
}

- (void)settingChanged:(id)sender {
  [self storeChoices];
}

- (void)languageChanged:(id)sender {
  _settings.language = _language.indexOfSelectedItem == 1 ? "es" : "en";
  [self storeChoices];
  [self renderTexts];
  [self fitWindow];
}

- (void)pickFileWithType:(NSString*)type then:(void (^)(NSString* path))then {
  NSOpenPanel* panel = [NSOpenPanel openPanel];
  panel.allowedFileTypes = @[ type ];
  panel.allowsMultipleSelection = NO;
  [panel beginSheetModalForWindow:_window
                completionHandler:^(NSModalResponse response) {
                  if (response == NSModalResponseOK) then(panel.URL.path);
                }];
}

- (void)chooseChd:(id)sender {
  [self pickFileWithType:@"chd"
                    then:^(NSString* path) {
                      [self runJob:NO path:path];
                    }];
}

- (void)addSounds:(id)sender {
  [self pickFileWithType:@"u9"
                    then:^(NSString* path) {
                      [self runJob:YES path:path];
                    }];
}

- (void)showPercent:(int)percent {
  _progress.doubleValue = percent;
  [self setStatus:@"extracting" argument:[NSString stringWithFormat:@"%d", percent] warn:NO];
}

- (void)runJob:(BOOL)sounds path:(NSString*)path {
  _busy = YES;
  _progress.doubleValue = 0;
  [self setStatus:sounds ? @"readingSounds" : @"opening" argument:nil warn:NO];
  [self renderControls];
  [self fitWindow];
  const std::string file = path.fileSystemRepresentation;
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
    const auto progress = [self](int percent) {
      dispatch_async(dispatch_get_main_queue(), ^{
        [self showPercent:percent];
      });
    };
    const std::string error = sounds ? piu::desktop::extract_sounds(file, progress) : piu::desktop::extract_game(file, progress);
    NSString* failure = [NSString stringWithUTF8String:error.c_str()];
    dispatch_async(dispatch_get_main_queue(), ^{
      self->_busy = NO;
      [self showLibrary];
      if (failure.length > 0) [self setStatus:failure argument:nil warn:YES];
    });
  });
}

- (void)launch:(id)sender {
  [self storeChoices];
  [_window orderOut:nil];
  piu::desktop::open_application(piu::desktop::Start::kGame);
  [NSApp terminate:nil];
}

- (void)mapKeys:(id)sender {
  [self storeChoices];
  [_window orderOut:nil];
  piu::desktop::open_application(piu::desktop::Start::kKeyMap);
  [NSApp terminate:nil];
}

@end

namespace piu::desktop {

void open_application(Start start) {
  @autoreleasepool {
    std::string bundle = NSBundle.mainBundle.bundlePath.fileSystemRepresentation;
    char open_name[] = "open";
    char new_instance[] = "-n";
    char pass_args[] = "--args";
    char play_arg[] = "--play";
    char keys_arg[] = "--keys";
    std::vector<char*> arguments = {open_name, new_instance, bundle.data()};
    if (start != Start::kLauncher) {
      arguments.push_back(pass_args);
      arguments.push_back(start == Start::kGame ? play_arg : keys_arg);
    }
    arguments.push_back(nullptr);
    pid_t child = 0;
    if (posix_spawn(&child, "/usr/bin/open", nullptr, nullptr, arguments.data(), environ) == 0) {
      waitpid(child, nullptr, 0);
    }
  }
}

void bring_application_to_front() {
  @autoreleasepool {
    [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
  }
}

int run_launcher(int, char**) {
  @autoreleasepool {
    NSApplication* application = [NSApplication sharedApplication];
    PiuLauncher* launcher = [[PiuLauncher alloc] init];
    application.delegate = launcher;
    [application setActivationPolicy:NSApplicationActivationPolicyRegular];
    [application run];
  }
  return 0;
}

}  // namespace piu::desktop
