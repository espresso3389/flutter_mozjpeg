# flutter_mozjpeg

A Flutter wrapper for mozjpeg.

## Getting Started

## iOS

The pod `mozjpeg` must be redirected to [my own fork](https://github.com/espresso3389/mozjpeg) just now. Add the following line to your `ios/Podfile`:

```
pod 'mozjpeg', :git => "https://github.com/espresso3389/mozjpeg.git"
```

Your `Podfile` may be like the following after adding the line:

```ruby
...

target 'Runner' do
  use_frameworks!
  use_modular_headers!

  pod 'mozjpeg', :git => "https://github.com/espresso3389/mozjpeg.git"

  flutter_install_all_ios_pods File.dirname(File.realpath(__FILE__))
end

...
```

## Android

Nothing to do.


## Example

```dart
import 'package:flutter_mozjpeg/flutter_mozjpeg.dart';

...

await FlutterMozjpeg.jpegCompressFileToFile(File('input.jpg'), File('output.jpg'));
```
