// You have generated a new plugin project without
// specifying the `--platforms` flag. A plugin project supports no platforms is generated.
// To add platforms, run `flutter create -t plugin --platforms <platforms> .` under the same
// directory. You can also find a detailed instruction on how to add platforms in the `pubspec.yaml` at https://flutter.dev/docs/development/packages-and-plugins/developing-packages#plugin-platforms.

// ignore_for_file: camel_case_types

import 'dart:async';
import 'dart:ffi';
import 'dart:io';
import 'dart:isolate';
import 'dart:typed_data';
import 'dart:ui' as ui;

import 'package:ffi/ffi.dart';

typedef Dart_InitializeApiDLFunc = Pointer<Void> Function(Pointer<Void>);

typedef MessageCallback = void Function(String);
typedef ProgressCallback = void Function(int pass, int totalPass, int percentage);

typedef _MainTypeFunc = int Function(int argc, Pointer<Pointer<Utf8>> argv, int context);

typedef _JpegCompressFunc = void Function(Pointer<Uint8>, int, int, int, int, int, int, int);

typedef _SetDartPortFunc = void Function(int port);

/// A Flutter wrapper of Mozilla JPEG Encoder ([mozjpeg](https://github.com/mozilla/mozjpeg)).
abstract class FlutterMozjpeg {
  static final mozJpegLib =
      Platform.isAndroid ? DynamicLibrary.open("libflutter_mozjpeg.so") : DynamicLibrary.process();

  /// [How to use async callback between C++ and Dart with FFI?](https://github.com/flutter/flutter/issues/63255)
  /// - Copy dart-sdk's header/impl. files and call `Dart_InitializeApiDL` with [NativeApi.initializeApiDLData](https://api.flutter.dev/flutter/dart-ffi/NativeApi/initializeApiDLData.html).
  static final Dart_InitializeApiDLFunc _Dart_InitializeApiDL =
      mozJpegLib.lookup<NativeFunction<Dart_InitializeApiDLFunc>>("Dart_InitializeApiDL").asFunction();

  /// Call to C++ implemented routine `set_dart_port`.
  static final _SetDartPortFunc _setDartPort =
      mozJpegLib.lookup<NativeFunction<Void Function(Int64)>>("set_dart_port").asFunction();

  static Pointer<Void>? cookie;

  static const int _PROGRESS_PASS_EXITCODE = -1;
  static const int _PROGRESS_PASS_OUTPUT_FILESIZE = -2;
  static const int PROGRESS_PASS_VECTOR_PTR = -3;
  static const int _PROGRESS_TPASS_OPTIMIZED = 1;
  static const int _PROGRESS_TPASS_ORIGINAL = 2;

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
          _progressCallbacks[context]?.call(pass, message[2] as int, message[3] as int);
          if (pass == _PROGRESS_PASS_EXITCODE) {
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

  static final _MainTypeFunc _jpegtran = mozJpegLib
      .lookup<NativeFunction<Int32 Function(Int32, Pointer<Pointer<Utf8>>, IntPtr)>>("jpegtran_threaded")
      .asFunction();

  static int _callMain(
    _MainTypeFunc func,
    List<String> args,
    String exeName,
    int context,
  ) {
    _ensureDartApiInitialized();

    final argv = calloc.allocate<Pointer<Utf8>>((args.length + 2) * sizeOf<Pointer<Utf8>>());
    try {
      argv[0] = exeName.toNativeUtf8();
      for (int i = 0; i < args.length; i++) {
        argv[i + 1] = args[i].toNativeUtf8();
      }

      return func(
        args.length + 1,
        argv,
        context,
      );
    } finally {
      for (int i = 0; i <= args.length; i++) {
        malloc.free(argv[i]);
      }
      calloc.free(argv);
    }
  }

  /// Execute `jpegtran` process with [args].
  /// It returns 0 if succeeded; other value indicates some error.
  /// The last parameter can be `@buffer@:address,size` to do conversion on memory. In that case, the return
  /// value is the resulting JPEG size on the buffer.
  static Future<int> jpegtran(
    List<String> args, {
    ProgressCallback? progressCallback,
  }) async {
    final comp = Completer<int>();
    _callMain(_jpegtran, args, "jpegtran", _addProgressCallback((pass, totalPass, percentage) {
      if (pass == _PROGRESS_PASS_EXITCODE || pass == _PROGRESS_PASS_OUTPUT_FILESIZE) {
        comp.complete(percentage);
        return;
      }
      progressCallback?.call(pass, totalPass, percentage);
    }));
    return await comp.future;
  }

  static final _JpegCompressFunc _jpegCompress = mozJpegLib
      .lookup<NativeFunction<Void Function(Pointer<Uint8>, Int32, Int32, Int32, Int32, Int32, Int32, IntPtr)>>(
          "jpeg_compress_threaded")
      .asFunction();
  static final Pointer<Uint8> Function(int) _jpegCompressGetPtr =
      mozJpegLib.lookup<NativeFunction<Pointer<Uint8> Function(IntPtr)>>("jpeg_compress_get_ptr").asFunction();
  static final int Function(int) _jpegCompressGetSize =
      mozJpegLib.lookup<NativeFunction<IntPtr Function(IntPtr)>>("jpeg_compress_get_size").asFunction();
  static final void Function(int) _jpegCompressRelease =
      mozJpegLib.lookup<NativeFunction<Void Function(IntPtr)>>("jpeg_compress_release").asFunction();

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
    _jpegCompress(src, width, height, stride, _cs2int[colorSpace]!, quality, dpi, _addProgressCallback(
      (pass, totalPass, percentage) {
        if (pass == _PROGRESS_PASS_EXITCODE) {
          if (percentage != 0) comp.complete(null);
          return;
        }
        if (pass == PROGRESS_PASS_VECTOR_PTR) {
          comp.complete(MozJpegEncodedResult._(percentage));
          return;
        }

        progressCallback?.call(pass, totalPass, percentage);
      },
    ));
    return await comp.future;
  }

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

  static Future<MozJpegEncodedResult?> jpegCompressFile(
    File file, {
    int quality = 75,
    int dpi = 96,
    ProgressCallback? progressCallback,
  }) async {
    return jpegCompressImage(
      await loadImageFromBytes(await file.readAsBytes()),
      quality: quality,
      dpi: dpi,
      progressCallback: progressCallback,
    );
  }
}

/// You must dispose the instance after use it.
class MozJpegEncodedResult {
  int? _vectorAddress;
  MozJpegEncodedResult._(this._vectorAddress);

  Uint8List get buffer => pointer.asTypedList(size);

  Pointer<Uint8> get pointer => FlutterMozjpeg._jpegCompressGetPtr(_vectorAddress!);

  int get size => FlutterMozjpeg._jpegCompressGetSize(_vectorAddress!);

  Future<void> save(File file) => file.writeAsBytes(buffer);

  Future<ui.Image> createImage() => loadImageFromBytes(buffer);

  void dispose() {
    FlutterMozjpeg._jpegCompressRelease(_vectorAddress!);
    _vectorAddress = null;
  }
}

final _cs2int = <MozJpegColorSpace, int>{
  MozJpegColorSpace.Unknown: 0,
  MozJpegColorSpace.Grayscale: 1,
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

enum MozJpegColorSpace {
  Unknown,
  Grayscale,
  RGB,
  YCbCr,
  CMYK,
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
