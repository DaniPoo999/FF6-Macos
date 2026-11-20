#include "ff6/content.hpp"
#include "ff6/io.hpp"
#include "ff6/rom_material.hpp"
#include "builder_icon.hpp"
#import <AppKit/AppKit.h>
#include <array>
#include <atomic>
#include <cerrno>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdio.h>
#include <system_error>
#include <sys/stat.h>

namespace {
namespace fs = std::filesystem;

void signing_task(NSArray<NSString*>* arguments) {
    NSTask* task = [[NSTask alloc] init];
    task.executableURL = [NSURL fileURLWithPath:@"/usr/bin/codesign"];
    task.arguments = arguments;
    NSPipe* errors = [NSPipe pipe];
    task.standardError = errors;
    NSError* launch_error = nil;
    if (![task launchAndReturnError:&launch_error]) {
        throw std::runtime_error(launch_error.localizedDescription.UTF8String);
    }
    NSData* data = [errors.fileHandleForReading readDataToEndOfFile];
    [task waitUntilExit];
    if (task.terminationStatus != 0) {
        NSString* message = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
        throw std::runtime_error(message ? message.UTF8String : "Code signing failed");
    }
}

std::array<fs::path, 3> build_apps(const fs::path& rom_path, const fs::path& folder,
                                  const std::function<bool()>& cancelled,
                                  const std::function<void(const std::string&)>& progress) {
    const auto checkpoint = [&] {
        if (cancelled())
            throw std::runtime_error("Build cancelled; no completed output was published.");
    };
    if (!fs::is_directory(folder))
        throw std::runtime_error("Choose an existing output folder.");
    const std::array<fs::path, 3> destinations{
        folder / "Final Fantasy VI.app", folder / "FFVI Content Inspector.app",
        folder / "FFVI Dependency Notices.txt"};
    for (const auto& destination : destinations)
        if (fs::symlink_status(destination).type() != fs::file_type::not_found)
            throw std::runtime_error(destination.filename().string() +
                                     " already exists. Choose another folder.");
    checkpoint();
    progress("Validating the supplied ROM…");
    const auto rom = ff6::validate_rom(ff6::read_file(rom_path, ff6::rom_size + 512));
    checkpoint();
    progress("Extracting verified data resources…");
    const auto pack = ff6::extract(rom);
    const auto bytes = ff6::encode_pack(pack);
    ff6::verify_canonical(ff6::decode_pack(bytes));
    const auto manifest = ff6::manifest_json(pack);
    const auto native_materials = ff6::extract_rom_materials(rom);
    checkpoint();

    const auto staging = folder / (std::string(".ffvi-build-") + [NSUUID UUID].UUIDString.UTF8String);
    if (!fs::create_directory(staging))
        throw std::runtime_error("Could not create a private build directory.");
    struct Cleanup {
        fs::path path;
        ~Cleanup() {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    } cleanup{staging};
    const std::array<NSString*, 2> templates{@"GameTemplate.app", @"InspectorTemplate.app"};
    std::array<fs::path, 3> staged;
    std::array<struct stat, 3> identities{};
    // Notices travel beside both apps, keeping their signed contents unchanged.
    // Stage them first: a missing resource must not publish either application.
    const fs::path builder_resources(NSBundle.mainBundle.resourcePath.UTF8String);
    staged[2] = staging / destinations[2].filename();
    ff6::write_new_file(staged[2], ff6::read_file(
        builder_resources / destinations[2].filename(), 256 * 1024));
    if (::lstat(staged[2].c_str(), &identities[2]) != 0)
        throw std::system_error(errno, std::generic_category(), "Checking dependency notices");
    for (std::size_t i = 0; i < templates.size(); ++i) {
        staged[i] = staging / destinations[i].filename();
        progress("Assembling " + destinations[i].filename().string() + "…");
        NSString* source = [NSBundle.mainBundle.resourcePath stringByAppendingPathComponent:templates[i]];
        NSError* copy_error = nil;
        if (![NSFileManager.defaultManager copyItemAtPath:source
                 toPath:[NSString stringWithUTF8String:staged[i].c_str()] error:&copy_error])
            throw std::runtime_error(copy_error.localizedDescription.UTF8String);
        progress("Restoring native tables from the supplied ROM…");
        const std::string binary = i == 0 ? "ff6-game" : "ff6-inspector";
        const auto executable = staged[i] / "Contents/MacOS" / binary;
        const fs::path resources(NSBundle.mainBundle.resourcePath.UTF8String);
        const auto restoration = ff6::decode_native_restoration(
            ff6::read_file(resources / (binary + ".ff6restore")));
        const auto restored = ff6::restore_native_image(ff6::read_file(executable),
                                                       restoration, native_materials);
        const auto replacement = executable.string() + ".restored";
        ff6::write_new_file(replacement, restored);
        fs::permissions(replacement, fs::status(executable).permissions());
        fs::rename(replacement, executable);
        checkpoint();
        ff6::write_new_file(staged[i] / "Contents/Resources/content.ff6pack", bytes);
        ff6::write_new_file(staged[i] / "Contents/Resources/content-manifest.json",
                           ff6::Bytes(manifest.begin(), manifest.end()));
        checkpoint();
        if (i == 0) {
            progress("Creating the game icon from the supplied ROM…");
            ff6::install_game_icon(pack, staged[i]);
            checkpoint();
        }
        progress("Signing and verifying " + destinations[i].filename().string() + "…");
        NSString* app = [NSString stringWithUTF8String:staged[i].c_str()];
        signing_task(@[ @"--force", @"--sign", @"-", app ]);
        signing_task(@[ @"--verify", @"--deep", @"--strict", app ]);
        ff6::verify_canonical(ff6::decode_pack(
            ff6::read_file(staged[i] / "Contents/Resources/content.ff6pack")));
        if (::lstat(staged[i].c_str(), &identities[i]) != 0)
            throw std::system_error(errno, std::generic_category(), "Checking completed app");
        checkpoint();
    }
    // Both apps and their notices are ready before publication. Each rename is atomic and refuses
    // existing paths. Do not admit cancellation halfway through this short commit.
    std::size_t published = 0;
    try {
        for (; published < staged.size(); ++published)
            if (::renamex_np(staged[published].c_str(), destinations[published].c_str(), RENAME_EXCL) != 0)
                throw std::system_error(errno, std::generic_category(), "Publishing completed apps");
    } catch (...) {
        // Roll back only our own output if any later destination raced us.
        // Never remove a pre-existing file/app or another process's replacement.
        for (std::size_t i = 0; i < published; ++i) {
            struct stat current{};
            if (::lstat(destinations[i].c_str(), &current) == 0 &&
                current.st_dev == identities[i].st_dev && current.st_ino == identities[i].st_ino)
                if (::renamex_np(destinations[i].c_str(), staged[i].c_str(), RENAME_EXCL) != 0)
                    throw std::runtime_error("Publication interrupted; a completed app remains in the output folder.");
        }
        throw;
    }
    return destinations;
}
} // namespace

@interface BuilderDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate> {
    std::atomic_bool cancellation_;
}
@property(strong) NSWindow* window;
@property(strong) NSTextField* romField;
@property(strong) NSTextField* outputField;
@property(strong) NSTextField* status;
@property(strong) NSButton* buildButton;
@property(strong) NSButton* romButton;
@property(strong) NSButton* folderButton;
@property(strong) NSButton* cancelButton;
@property(strong) NSProgressIndicator* progress;
@property BOOL busy;
@end

@implementation BuilderDelegate
- (NSTextField*)label:(NSString*)text frame:(NSRect)frame {
    NSTextField* label = [NSTextField labelWithString:text];
    label.frame = frame;
    [self.window.contentView addSubview:label];
    return label;
}
- (NSButton*)button:(NSString*)title action:(SEL)action frame:(NSRect)frame {
    NSButton* button = [NSButton buttonWithTitle:title target:self action:action];
    button.frame = frame;
    [self.window.contentView addSubview:button];
    return button;
}
- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    (void)notification;
    cancellation_ = false;
    self.window =
        [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 650, 330)
                                    styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                              NSWindowStyleMaskMiniaturizable
                                      backing:NSBackingStoreBuffered
                                        defer:NO];
    NSString* version = [NSBundle.mainBundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
    self.window.title = [NSString stringWithFormat:@"FFVI Builder %@ — Prerelease", version];
    self.window.delegate = self;
    [self.window center];
    NSTextField* heading = [self label:@"Build Final Fantasy VI"
                                 frame:NSMakeRect(24, 278, 600, 28)];
    heading.font = [NSFont boldSystemFontOfSize:22];
    [self label:@"Creates the game and a content inspector. Prerelease: stability is not established."
          frame:NSMakeRect(24, 244, 602, 24)];
    self.romField = [self label:@"Choose a Final Fantasy III USA Rev 1 ROM"
                          frame:NSMakeRect(24, 192, 450, 24)];
    self.romField.lineBreakMode = NSLineBreakByTruncatingMiddle;
    self.romButton = [self button:@"Choose ROM…"
                           action:@selector(chooseRom:)
                            frame:NSMakeRect(487, 187, 140, 32)];
    self.outputField = [self label:@"Choose the output folder" frame:NSMakeRect(24, 145, 450, 24)];
    self.outputField.lineBreakMode = NSLineBreakByTruncatingMiddle;
    self.folderButton = [self button:@"Choose Folder…"
                              action:@selector(chooseFolder:)
                               frame:NSMakeRect(487, 140, 140, 32)];
    self.progress = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(24, 108, 600, 8)];
    self.progress.indeterminate = YES;
    [self.window.contentView addSubview:self.progress];
    self.status = [self label:@"Works offline. The ROM is read only."
                        frame:NSMakeRect(24, 68, 600, 30)];
    self.status.lineBreakMode = NSLineBreakByTruncatingTail;
    self.buildButton = [self button:@"Build Apps"
                             action:@selector(build:)
                              frame:NSMakeRect(460, 20, 168, 34)];
    self.cancelButton = [self button:@"Cancel"
                              action:@selector(cancel:)
                               frame:NSMakeRect(354, 20, 100, 34)];
    self.cancelButton.enabled = NO;
    [self.window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}
- (void)chooseRom:(id)sender {
    (void)sender;
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = YES;
    panel.canChooseDirectories = NO;
    panel.allowsMultipleSelection = NO;
    if ([panel runModal] == NSModalResponseOK)
        self.romField.stringValue = panel.URL.path;
}
- (void)chooseFolder:(id)sender {
    (void)sender;
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = NO;
    panel.canChooseDirectories = YES;
    panel.canCreateDirectories = YES;
    if ([panel runModal] == NSModalResponseOK)
        self.outputField.stringValue = panel.URL.path;
}
- (void)setBusyControls:(BOOL)busy {
    self.busy = busy;
    self.buildButton.enabled = !busy;
    self.romButton.enabled = !busy;
    self.folderButton.enabled = !busy;
    self.cancelButton.enabled = busy;
    if (busy)
        [self.progress startAnimation:nil];
    else
        [self.progress stopAnimation:nil];
}
- (void)build:(id)sender {
    (void)sender;
    if (self.busy)
        return;
    const fs::path rom(self.romField.stringValue.UTF8String);
    const fs::path folder(self.outputField.stringValue.UTF8String);
    cancellation_ = false;
    [self setBusyControls:YES];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
      @autoreleasepool {
          NSString* message;
          NSArray<NSURL*>* completedURLs = nil;
          try {
              auto output = build_apps(
                  rom, folder, [&] { return self->cancellation_.load(); },
                  [&](const std::string& text) {
                      NSString* update = [NSString stringWithUTF8String:text.c_str()];
                      dispatch_async(dispatch_get_main_queue(), ^{
                        self.status.stringValue = update;
                      });
                  });
              completedURLs = @[
                  [NSURL fileURLWithPath:[NSString stringWithUTF8String:output[0].c_str()]],
                  [NSURL fileURLWithPath:[NSString stringWithUTF8String:output[1].c_str()]] ];
              message = @"Built both apps and their companion dependency notices.";
          } catch (const std::exception& error) {
              message = [NSString stringWithUTF8String:error.what()];
          }
          dispatch_async(dispatch_get_main_queue(), ^{
            [self setBusyControls:NO];
            self.status.stringValue = message;
            self.status.toolTip = message;
            if (completedURLs)
                [NSWorkspace.sharedWorkspace
                    activateFileViewerSelectingURLs:completedURLs];
            else {
                NSAlert* alert = [[NSAlert alloc] init];
                alert.messageText = @"Build did not complete";
                alert.informativeText = message;
                [alert beginSheetModalForWindow:self.window completionHandler:nil];
            }
          });
      }
    });
}
- (void)cancel:(id)sender {
    (void)sender;
    cancellation_ = true;
}
- (BOOL)windowShouldClose:(NSWindow*)sender {
    (void)sender;
    if (self.busy) {
        cancellation_ = true;
        return NO;
    }
    return YES;
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    (void)sender;
    return YES;
}
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender {
    (void)sender;
    if (self.busy) {
        cancellation_ = true;
        return NSTerminateCancel;
    }
    return NSTerminateNow;
}
@end

int main(int argc, char** argv) {
    @autoreleasepool {
        // The CLI exercises exactly the same offline packaging path as the GUI.
        if (argc == 4 && (std::string_view(argv[1]) == "--build" ||
                          std::string_view(argv[1]) == "--build-inspector")) {
            try {
                const auto output = build_apps(
                    argv[2], argv[3], [] { return false; },
                    [](const std::string& text) { std::cout << text << '\n'; });
                for (const auto& app : output)
                    std::cout << app << '\n';
                return 0;
            } catch (const std::exception& error) {
                std::cerr << error.what() << '\n';
                return 1;
            }
        }
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        BuilderDelegate* delegate = [[BuilderDelegate alloc] init];
        NSApp.delegate = delegate;
        [NSApp run];
    }
    return 0;
}
