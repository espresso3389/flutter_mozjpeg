#
# To learn more about a Podspec see http://guides.cocoapods.org/syntax/podspec.html.
# Run `pod lib lint flutter_mozjpeg.podspec` to validate before publishing.
#
Pod::Spec.new do |s|
  s.name             = 'flutter_mozjpeg'
  s.version          = '0.0.1'
  s.summary          = 'A new flutter plugin project.'
  s.description      = <<-DESC
A new flutter plugin project.
                       DESC
  s.homepage         = 'http://example.com'
  s.license          = { :file => '../LICENSE' }
  s.author           = { 'Your Company' => 'email@example.com' }
  s.source           = { :path => '.' }
  s.source_files = 'Classes/**/*'
  s.private_header_files = 'Classes/dart-sdk/internal/dart_api_dl_impl.h',
                           'Classes/dart-sdk/dart_api_dl.h',
                           'Classes/dart-sdk/dart_native_api.h',
                           'Classes/dart-sdk/dart_tools_api.h',
                           'Classes/dart-sdk/dart_version.h',
                           'Classes/cderror.h',
                           'Classes/cdjpeg.h'

  s.public_header_files = 'Classes/cdjapi.h'
  s.compiler_flags = '-Wno-strict-prototypes'
  s.dependency 'Flutter'
  s.dependency 'mozjpeg', '~> 4.0.3'
  s.platform = :ios, '11.0'

  # Flutter.framework does not contain a i386 slice.
  s.pod_target_xcconfig = { 'DEFINES_MODULE' => 'YES', 'EXCLUDED_ARCHS[sdk=iphonesimulator*]' => 'i386' }
  s.swift_version = '5.0'
end
