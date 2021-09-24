#import "FlutterMozjpegPlugin.h"
#if __has_include(<flutter_mozjpeg/flutter_mozjpeg-Swift.h>)
#import <flutter_mozjpeg/flutter_mozjpeg-Swift.h>
#else
// Support project import fallback if the generated compatibility header
// is not copied when this plugin is created as a library.
// https://forums.swift.org/t/swift-static-libraries-dont-copy-generated-objective-c-header/19816
#import "flutter_mozjpeg-Swift.h"
#endif

@implementation FlutterMozjpegPlugin
+ (void)registerWithRegistrar:(NSObject<FlutterPluginRegistrar>*)registrar {
  [SwiftFlutterMozjpegPlugin registerWithRegistrar:registrar];
}
@end
