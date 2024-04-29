import 'dart:async';
import 'dart:ffi';
import 'dart:io';
import 'dart:isolate';
import 'dart:typed_data';
import 'dart:ui' as ui;

import 'package:ffi/ffi.dart';

typedef _Dart_InitializeApiDLFunc = Pointer<Void> Function(Pointer<Void>);
typedef _JpegCompressFunc = void Function(
    Pointer<Uint8>, int, int, int, int, int, int, int);
typedef _SetDartPortFunc = void Function(int port);

typedef MessageCallback = void Function(String);

/// Progress callback receives [pass], [totalPass] and the progress [percentage] (%) on the pass.
typedef ProgressCallback = void Function(
    int pass, int totalPass, int percentage);

/// A Flutter wrapper of Mozilla JPEG Encoder ([mozjpeg](https://github.com/mozilla/mozjpeg)).
abstract class FlutterMozjpeg {
  static final mozJpegLib = Platform.isAndroid
      ? DynamicLibrary.open("libflutter_mozjpeg.so")
      : DynamicLibrary.process();

  /// [How to use async callback between C++ and Dart with FFI?](https://github.com/flutter/flutter/issues/63255)
  /// - Copy dart-sdk's header/impl. files and call `Dart_InitializeApiDL` with [NativeApi.initializeApiDLData](https://api.flutter.dev/flutter/dart-ffi/NativeApi/initializeApiDLData.html).
  // ignore: non_constant_identifier_names
  static final _Dart_InitializeApiDLFunc _Dart_InitializeApiDL = mozJpegLib
      .lookup<NativeFunction<_Dart_InitializeApiDLFunc>>("Dart_InitializeApiDL")
      .asFunction();

  /// Call to C++ implemented routine `set_dart_port`.
  static final _SetDartPortFunc _setDartPort = mozJpegLib
      .lookup<NativeFunction<Void Function(Int64)>>("set_dart_port")
      .asFunction();

  static Pointer<Void>? cookie;

  static const int _progressPassExitCode = -1;
  static const int _progressPassOutputFileSize = -2;
  static const int _progressPassVectorPointer = -3;
  static const int _progressTPassOptimized = 1;
  static const int _progressTPassNoChange = 2;

  static void _ensureDartApiInitialized() {
    if (cookie != null) return;
    cookie = _Dart_InitializeApiDL(NativeApi.initializeApiDLData);
    final pub = ReceivePort()
      ..listen((message) {
        if (message is String) {
          messageCallback?.call(message);
          return;
        }
        if (message is List && message.length == 4) {
          // 0:context, 1:pass, 2:totalPass, 3:percentage
          int context = message[0] as int;
          int pass = message[1] as int;
          // lookup progress callback associated to the context value and invoke it with the parameters
          _progressCallbacks[context]
              ?.call(pass, message[2] as int, message[3] as int);
          if (pass == _progressPassExitCode) {
            _progressCallbacks.remove(context);
          }
          return;
        }
        if (message is int) {}
      });
    _setDartPort(pub.sendPort.nativePort);
  }

  /// Callback that receives log messages from mozjpeg library.
  static MessageCallback? messageCallback;

  /// context value to progress callback map
  static final _progressCallbacks = <int, ProgressCallback?>{};
  static int _pcnIndex = 0;

  /// register a progress callback and return it's context value that is used as key of [_progressCallbacks].
  static int _addProgressCallback(ProgressCallback? progressCallback) {
    final int context = ++_pcnIndex;
    _progressCallbacks[context] = progressCallback;
    return context;
  }

  static final _JpegCompressFunc _jpegCompress = mozJpegLib
      .lookup<
          NativeFunction<
              Void Function(Pointer<Uint8>, Int32, Int32, Int32, Int32, Int32,
                  Int32, IntPtr)>>("jpeg_compress_threaded")
      .asFunction();
  static final Pointer<Uint8> Function(int) _jpegCompressGetPtr = mozJpegLib
      .lookup<NativeFunction<Pointer<Uint8> Function(IntPtr)>>(
          "jpeg_compress_get_ptr")
      .asFunction();
  static final int Function(int) _jpegCompressGetSize = mozJpegLib
      .lookup<NativeFunction<IntPtr Function(IntPtr)>>("jpeg_compress_get_size")
      .asFunction();
  static final void Function(int) _jpegCompressRelease = mozJpegLib
      .lookup<NativeFunction<Void Function(IntPtr)>>("jpeg_compress_release")
      .asFunction();

  /// Compress the raw image data on memory.
  /// [stride], a.k.a. bytes-per-line, is depending on the pixel layout. If the data is RGBA,
  /// [stride] is typically `width * 4` unless there are any trailing padding bytes.
  /// [quality] is JPEG compression quality in [0 - 100]; the default is 75.
  /// [dpi] is just an additional metadata, dot-per-inch; the default is 96.
  /// [progressCallback] receives progress percentage during the conversion.
  static Future<MozJpegEncodedResult?> jpegCompress(
    Pointer<Uint8> src,
    int width,
    int height,
    int stride,
    MozJpegColorSpace colorSpace, {
    int quality = 75,
    int dpi = 96,
    ProgressCallback? progressCallback,
  }) async {
    _ensureDartApiInitialized();
    final comp = Completer<MozJpegEncodedResult?>();
    _jpegCompress(
        src, width, height, stride, _cs2int[colorSpace]!, quality, dpi,
        _addProgressCallback(
      (pass, totalPass, percentage) {
        if (pass == _progressPassExitCode) {
          if (percentage != 0) comp.complete(null);
          return;
        }
        if (pass == _progressPassVectorPointer) {
          comp.complete(MozJpegEncodedResult._(percentage));
          return;
        }

        progressCallback?.call(pass, totalPass, percentage);
      },
    ));
    return await comp.future;
  }

  /// [quality] is JPEG compression quality in [0 - 100]; the default is 75.
  /// [dpi] is just an additional metadata, dot-per-inch; the default is 96.
  /// [progressCallback] receives progress percentage during the conversion.
  static Future<MozJpegEncodedResult?> jpegCompressImage(
    ui.Image image, {
    int quality = 75,
    int dpi = 96,
    ProgressCallback? progressCallback,
  }) =>
      image.compressWithMozJpeg(
        quality: quality,
        dpi: dpi,
        progressCallback: progressCallback,
      );

  /// [quality] is JPEG compression quality in [0 - 100]; the default is 75.
  /// [dpi] is just an additional metadata, dot-per-inch; the default is 96.
  /// [progressCallback] receives progress percentage during the conversion.
  static Future<MozJpegEncodedResult?> jpegCompressFileBytes(
    Uint8List fileBytes, {
    int quality = 75,
    int dpi = 96,
    ProgressCallback? progressCallback,
  }) async {
    return jpegCompressImage(
      await loadImageFromBytes(fileBytes),
      quality: quality,
      dpi: dpi,
      progressCallback: progressCallback,
    );
  }

  /// [quality] is JPEG compression quality in [0 - 100]; the default is 75.
  /// [dpi] is just an additional metadata, dot-per-inch; the default is 96.
  /// [progressCallback] receives progress percentage during the conversion.
  static Future<MozJpegEncodedResult?> jpegCompressFile(
    File file, {
    int quality = 75,
    int dpi = 96,
    ProgressCallback? progressCallback,
  }) async =>
      jpegCompressFileBytes(
        await file.readAsBytes(),
        quality: quality,
        dpi: dpi,
        progressCallback: progressCallback,
      );

  /// Compress the raw RGBA image data on memory.
  /// [stride], a.k.a. bytes-per-line, is depending on the pixel layout. If the data is RGBA,
  /// [stride] is typically `width * 4` unless there are any trailing padding bytes.
  /// [quality] is JPEG compression quality in [0 - 100]; the default is 75.
  /// [dpi] is just an additional metadata, dot-per-inch; the default is 96.
  /// [progressCallback] receives progress percentage during the conversion.
  static Future<MozJpegEncodedResult?> jpegCompressRgbaBytes(
    Uint8List rgba,
    int width,
    int height, {
    int? stride,
    int quality = 75,
    int dpi = 96,
    ProgressCallback? progressCallback,
  }) =>
      using((arena) async {
        final buffer = arena.allocate<Uint8>(rgba.lengthInBytes);
        final length = rgba.lengthInBytes;
        for (int i = 0; i < length; i++) {
          buffer[i] = rgba[i];
        }
        return await FlutterMozjpeg.jpegCompress(
          buffer,
          width,
          height,
          stride ?? width * 4,
          MozJpegColorSpace.extRGBX,
          quality: quality,
          dpi: dpi,
          progressCallback: progressCallback,
        );
      });

  /// [quality] is JPEG compression quality in [0 - 100]; the default is 75.
  /// [dpi] is just an additional metadata, dot-per-inch; the default is 96.
  /// [progressCallback] receives progress percentage during the conversion.
  static Future<bool> jpegCompressFileBytesToFile(
    Uint8List fileBytes,
    File output, {
    int quality = 75,
    int dpi = 96,
    ProgressCallback? progressCallback,
  }) async {
    final result = await jpegCompressFileBytes(
      fileBytes,
      quality: quality,
      dpi: dpi,
      progressCallback: progressCallback,
    );
    if (result == null) return false;
    try {
      await result.save(output);
      return true;
    } catch (e) {
      return false;
    } finally {
      result.dispose();
    }
  }

  /// [quality] is JPEG compression quality in [0 - 100]; the default is 75.
  /// [dpi] is just an additional metadata, dot-per-inch; the default is 96.
  /// [progressCallback] receives progress percentage during the conversion.
  static Future<bool> jpegCompressFileToFile(
    File input,
    File output, {
    int quality = 75,
    int dpi = 96,
    ProgressCallback? progressCallback,
  }) async {
    return jpegCompressFileBytesToFile(
      await input.readAsBytes(),
      output,
      quality: quality,
      dpi: dpi,
      progressCallback: progressCallback,
    );
  }
}

/// JPEG compression result.
/// You must call [dispose] after using it.
class MozJpegEncodedResult {
  int? _vectorAddress;
  MozJpegEncodedResult._(this._vectorAddress);

  /// Buffer that contains the compressed result.
  Uint8List get buffer => pointer.asTypedList(size);

  /// Pointer to the buffer that contains the compressed result.
  Pointer<Uint8> get pointer =>
      FlutterMozjpeg._jpegCompressGetPtr(_vectorAddress!);

  /// Size in bytes of the compressed result.
  int get size => FlutterMozjpeg._jpegCompressGetSize(_vectorAddress!);

  /// Save the compressed result JPEG data to file.
  Future<void> save(File file) => file.writeAsBytes(buffer);

  /// Create image object from the compressed result.
  Future<ui.Image> createImage() => loadImageFromBytes(buffer);

  /// Release the resources used by the object.
  void dispose() {
    FlutterMozjpeg._jpegCompressRelease(_vectorAddress!);
    _vectorAddress = null;
  }
}

final _cs2int = <MozJpegColorSpace, int>{
  MozJpegColorSpace.unknown: 0,
  MozJpegColorSpace.grayscale: 1,
  MozJpegColorSpace.RGB: 2,
  MozJpegColorSpace.YCbCr: 3,
  MozJpegColorSpace.CMYK: 4,
  MozJpegColorSpace.YCCK: 5,
  MozJpegColorSpace.extRGB: 6,
  MozJpegColorSpace.extRGBX: 7,
  MozJpegColorSpace.extBGR: 8,
  MozJpegColorSpace.extBGRX: 9,
  MozJpegColorSpace.extXBGR: 10,
  MozJpegColorSpace.extXRGB: 11,
  MozJpegColorSpace.extRGBA: 12,
  MozJpegColorSpace.extBGRA: 13,
  MozJpegColorSpace.extABGR: 14,
  MozJpegColorSpace.extARGB: 15,
  MozJpegColorSpace.RGB565: 16
};

/// Input color space and pixel layout.
enum MozJpegColorSpace {
  unknown,
  grayscale,
  // ignore: constant_identifier_names
  RGB,
  // ignore: constant_identifier_names
  YCbCr,
  // ignore: constant_identifier_names
  CMYK,
  // ignore: constant_identifier_names
  YCCK,
  extRGB,
  extRGBX,
  extBGR,
  extBGRX,
  extXBGR,
  extXRGB,
  extRGBA,
  extBGRA,
  extABGR,
  extARGB,
  // ignore: constant_identifier_names
  RGB565,
}

/// Helper function to load image from data bytes. It's just a wrapper function for [ui.decodeImageFromList].
Future<ui.Image> loadImageFromBytes(Uint8List byteData) {
  final comp = Completer<ui.Image>();
  ui.decodeImageFromList(byteData, (result) => comp.complete(result));
  return comp.future;
}

extension FlutterMozjpegOnUiImage on ui.Image {
  Future<MozJpegEncodedResult?> compressWithMozJpeg({
    int quality = 75,
    int dpi = 96,
    ProgressCallback? progressCallback,
  }) =>
      using((arena) async {
        final data = (await toByteData())!;
        final buffer = arena.allocate<Uint8>(data.lengthInBytes);
        final u8data = data.buffer.asUint8List();
        final length = u8data.lengthInBytes;
        for (int i = 0; i < length; i++) {
          buffer[i] = u8data[i];
        }
        return await FlutterMozjpeg.jpegCompress(
          buffer,
          width,
          height,
          width * 4,
          MozJpegColorSpace.extRGBX,
          quality: quality,
          dpi: dpi,
          progressCallback: progressCallback,
        );
      });
}
