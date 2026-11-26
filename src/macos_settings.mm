#include "macos_settings.hpp"
#include "game_presenter.hpp"
#import <AppKit/AppKit.h>

@interface FFVISettingsController : NSWindowController
- (instancetype)initWithPresenter:(presentation::GamePresenter*)presenter;
- (void)openSettings:(id)sender;
- (void)changeSettings:(id)sender;
@end

@implementation FFVISettingsController {
    presentation::GamePresenter* presenter_;
    NSButton *enabled_, *curvature_, *advanced_, *reset_;
    NSPopUpButton *shader_, *aspect_;
    NSView* advancedView_;
    NSTextField *heading_, *shaderLabel_, *advancedLabel_, *help_;
    NSSlider* sliders_[7];
    NSTextField* values_[7];
}
- (instancetype)initWithPresenter:(presentation::GamePresenter*)presenter {
    NSWindow* window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,480,280)
        styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
        backing:NSBackingStoreBuffered defer:NO];
    self=[super initWithWindow:window];
    if (self) {
        presenter_=presenter;
        window.title=@"Settings"; window.identifier=@"ffvi.settings";
        window.releasedWhenClosed=NO;
        // Keep Settings usable in the game's fullscreen Space.
        window.collectionBehavior=NSWindowCollectionBehaviorFullScreenAuxiliary;
        [window center];
        NSView* content=window.contentView;
        heading_=[NSTextField labelWithString:@"Graphics"];
        heading_.font=[NSFont boldSystemFontOfSize:14];
        enabled_=[NSButton checkboxWithTitle:@"Enable shaders" target:self action:@selector(changeSettings:)];
        enabled_.identifier=@"ffvi.shaders.enabled";
        shaderLabel_=[NSTextField labelWithString:@"CRT shader:"];
        shader_=[[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
        shader_.identifier=@"ffvi.shaders.preset";
        [shader_ setAccessibilityLabel:@"CRT shader"];
        [shader_ addItemsWithTitles:@[@"CRT SDR",@"CRT HDR"]];
        shader_.target=self; shader_.action=@selector(changeSettings:);
        curvature_=[NSButton checkboxWithTitle:@"Curved CRT screen" target:self action:@selector(changeSettings:)];
        curvature_.identifier=@"ffvi.shaders.curvature";
        advanced_=[NSButton buttonWithTitle:@"" target:self action:@selector(toggleAdvanced:)];
        advanced_.buttonType=NSButtonTypePushOnPushOff;
        advanced_.bezelStyle=NSBezelStyleDisclosure;
        advanced_.identifier=@"ffvi.shaders.advanced";
        [advanced_ setAccessibilityLabel:@"Advanced"];
        advancedLabel_=[NSTextField labelWithString:@"Advanced"];
        help_=[NSTextField wrappingLabelWithString:
            @"Changes apply immediately and are saved automatically. HDR uses SDR when your display cannot provide HDR."];
        help_.font=[NSFont systemFontOfSize:12]; help_.textColor=NSColor.secondaryLabelColor;
        for (NSView* view in @[heading_,enabled_,shaderLabel_,shader_,curvature_,advanced_,advancedLabel_,help_]) [content addSubview:view];
        advancedView_=[[NSView alloc] initWithFrame:NSMakeRect(24,70,432,352)];
        [content addSubview:advancedView_];
        auto* label=[NSTextField labelWithString:@"Aspect ratio:"];
        label.frame=NSMakeRect(0,320,140,22); [advancedView_ addSubview:label];
        aspect_=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(150,316,270,28) pullsDown:NO];
        [aspect_ addItemsWithTitles:@[@"8:7 — original pixels",@"4:3 — television"]];
        aspect_.identifier=@"ffvi.shaders.aspect"; [aspect_ setAccessibilityLabel:@"Aspect ratio"];
        aspect_.target=self; aspect_.action=@selector(changeSettings:); [advancedView_ addSubview:aspect_];
        NSArray<NSString*>* names=@[@"Curvature",@"Sharpness",@"Scanlines",@"Phosphor size",@"Phosphor strength",@"Glass glow",@"Brightness"];
        NSArray<NSString*>* ids=@[@"strength",@"sharpness",@"scanlines",@"size",@"mask",@"glow",@"brightness"];
        const double minimum[]={0,0,0,.5,0,0,.5}, maximum[]={1,1,1,2,1,.15,1.5};
        for (unsigned i=0;i<7;++i) {
            const CGFloat y=280-36*i;
            auto* title=[NSTextField labelWithString:names[i]];
            title.frame=NSMakeRect(0,y,140,22); [advancedView_ addSubview:title];
            sliders_[i]=[NSSlider sliderWithValue:0 minValue:minimum[i] maxValue:maximum[i]
                target:self action:@selector(changeSettings:)];
            sliders_[i].frame=NSMakeRect(150,y,216,24);
            sliders_[i].continuous=YES;
            sliders_[i].identifier=[@"ffvi.shaders." stringByAppendingString:ids[i]];
            [sliders_[i] setAccessibilityLabel:names[i]];
            [advancedView_ addSubview:sliders_[i]];
            values_[i]=[NSTextField labelWithString:@""];
            values_[i].frame=NSMakeRect(374,y,58,22);
            values_[i].alignment=NSTextAlignmentRight;
            values_[i].font=[NSFont monospacedDigitSystemFontOfSize:12 weight:NSFontWeightRegular];
            [advancedView_ addSubview:values_[i]];
        }
        reset_=[NSButton buttonWithTitle:@"Restore Defaults" target:self action:@selector(restoreDefaults:)];
        reset_.identifier=@"ffvi.shaders.defaults";
        reset_.frame=NSMakeRect(150,14,160,28); [advancedView_ addSubview:reset_];
        [self layoutSettings];
    }
    return self;
}
- (void)layoutSettings {
    const bool expanded=advanced_.state==NSControlStateValueOn;
    const CGFloat h=expanded ? 632 : 280;
    NSRect old=self.window.frame;
    [self.window setContentSize:NSMakeSize(480,h)];
    [self.window setFrameTopLeftPoint:NSMakePoint(old.origin.x,NSMaxY(old))];
    heading_.frame=NSMakeRect(24,h-46,432,22);
    enabled_.frame=NSMakeRect(24,h-86,432,24);
    shaderLabel_.frame=NSMakeRect(24,h-126,100,22);
    shader_.frame=NSMakeRect(150,h-130,300,28);
    curvature_.frame=NSMakeRect(24,h-166,432,24);
    advanced_.frame=NSMakeRect(24,h-204,18,18);
    advancedLabel_.frame=NSMakeRect(48,h-206,150,22);
    advancedView_.hidden=!expanded;
    help_.frame=NSMakeRect(24,16,432,48);
}
- (void)syncControls {
    const auto s=presenter_->settings();
    enabled_.state=s.enabled ? NSControlStateValueOn : NSControlStateValueOff;
    [shader_ selectItemAtIndex:s.shader==presentation::Mode::hdr ? 1 : 0];
    shader_.enabled=s.enabled; curvature_.enabled=s.enabled;
    curvature_.state=s.curvature ? NSControlStateValueOn : NSControlStateValueOff;
    [aspect_ selectItemAtIndex:s.aspect_4_3 ? 1 : 0];
    const float values[]={s.curvature_strength,s.sharpness,s.scanlines,s.phosphor_size,s.phosphor_strength,s.glow,s.brightness};
    for (unsigned i=0;i<7;++i) {
        sliders_[i].doubleValue=values[i];
        sliders_[i].enabled=s.enabled && (i!=0 || s.curvature);
        values_[i].stringValue=(i==3 || i==6) ? [NSString stringWithFormat:@"%.2f×",values[i]] :
                                                         [NSString stringWithFormat:@"%.0f%%",values[i]*100];
    }
}
- (void)openSettings:(id)sender {
    (void)sender; [self syncControls]; [self showWindow:nil]; [self.window makeKeyAndOrderFront:nil];
}
- (void)toggleAdvanced:(id)sender { (void)sender; [self layoutSettings]; }
- (void)restoreDefaults:(id)sender {
    (void)sender;
    presenter_->configure(presentation::Settings{}); [self syncControls];
}
- (void)changeSettings:(id)sender {
    (void)sender;
    auto s=presenter_->settings();
    s.enabled=enabled_.state==NSControlStateValueOn;
    s.shader=shader_.indexOfSelectedItem==1 ? presentation::Mode::hdr : presentation::Mode::sdr;
    s.curvature=curvature_.state==NSControlStateValueOn; s.aspect_4_3=aspect_.indexOfSelectedItem==1;
    s.curvature_strength=sliders_[0].floatValue; s.sharpness=sliders_[1].floatValue;
    s.scanlines=sliders_[2].floatValue; s.phosphor_size=sliders_[3].floatValue;
    s.phosphor_strength=sliders_[4].floatValue; s.glow=sliders_[5].floatValue; s.brightness=sliders_[6].floatValue;
    presenter_->configure(s); [self syncControls];
}
@end

namespace presentation {
namespace {
class MacSettings final : public SettingsWindow {
    FFVISettingsController* controller_;
    NSMenuItem* item_;
    __weak id previous_target_;
    SEL previous_action_{};
    NSString* previous_title_;
  public:
    explicit MacSettings(GamePresenter& presenter) {
        // SDL supplies Services, Hide and Quit. Replace only its disabled
        // Preferences placeholder, identified by the standard key equivalent.
        NSMenu* menu = NSApp.mainMenu.itemArray.firstObject.submenu;
        for (NSMenuItem* item in menu.itemArray) {
            if ([item.keyEquivalent isEqualToString:@","]) { item_ = item; break; }
        }
        if (!item_) return;
        previous_target_ = item_.target;
        previous_action_ = item_.action;
        previous_title_ = item_.title;
        controller_ = [[FFVISettingsController alloc] initWithPresenter:&presenter];
        item_.title = @"Settings…";
        item_.target = controller_;
        item_.action = @selector(openSettings:);
    }
    ~MacSettings() override {
        [controller_ close];
        item_.target = previous_target_;
        item_.action = previous_action_;
        if (previous_title_) item_.title = previous_title_;
    }
};
}
std::unique_ptr<SettingsWindow> make_settings_window(GamePresenter& presenter) {
    return std::make_unique<MacSettings>(presenter);
}
} // namespace presentation
