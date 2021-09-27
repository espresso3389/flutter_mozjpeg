import 'dart:io';

import 'package:flutter/material.dart';
import 'dart:async';
import 'dart:ui' as ui;

import 'package:flutter_mozjpeg/flutter_mozjpeg.dart';
import 'package:image_picker/image_picker.dart';
import 'package:path/path.dart' as path;
import 'package:path_provider/path_provider.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatefulWidget {
  const MyApp({Key? key}) : super(key: key);

  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  _ConversionResult? result;
  final textEditingController = TextEditingController();
  final progress = ValueNotifier<List<int>>([0, 0, 0]); // pass, totalPass, percentage

  @override
  void initState() {
    super.initState();
  }

  @override
  void dispose() {
    textEditingController.dispose();
    progress.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        appBar: AppBar(
          title: const Text('Plugin example app'),
        ),
        body: result != null
            ? Column(
                children: [
                  Text('Size: ${result!.inputSize} -> ${result!.outputSize}'),
                  InteractiveViewer(
                    child: Stack(
                      children: [
                        RawImage(key: Key(result!.file.path), image: result!.image),
                      ],
                    ),
                  ),
                  SizedBox(
                    height: 200,
                    child: TextField(
                      controller: textEditingController,
                      keyboardType: TextInputType.multiline,
                      maxLines: null,
                      readOnly: true,
                    ),
                  ),
                ],
              )
            : ValueListenableBuilder<List<int>>(
                valueListenable: progress,
                builder: (context, value, child) {
                  return Column(
                    children: [
                      Text('Pass ${value[0]}/${value[1]}: ${value[2]}%'),
                      LinearProgressIndicator(value: value[2] / 100),
                    ],
                  );
                }),
        floatingActionButton: FloatingActionButton(
          tooltip: 'Pick an Image',
          onPressed: _compressImage,
          child: const Icon(Icons.image),
        ),
      ),
    );
  }

  Future<void> _compressImage() async {
    final imageFile = await ImagePicker().pickImage(source: ImageSource.gallery, maxWidth: 2000, maxHeight: 2000);
    if (imageFile == null) return;

    final tmp = await getTemporaryDirectory();
    final outputImageFile = File(path.join(tmp.path, 'output.jpg'));
    FlutterMozjpeg.messageCallback = (m) => print('jpegtran: $m');
    final ret = FlutterMozjpeg.jpegtran(
      ['-copy', 'none', '-optimize', '-outfile', outputImageFile.path, imageFile.path],
      progressCallback: (pass, totalPass, percentage) {
        print('jpegtran: $pass/$totalPass/$percentage');
        progress.value = [pass, totalPass, percentage];
      },
    );

    result?.dispose();
    result = null;
    if (mounted) {
      setState(() {});
    }

    if (ret == 0) {
      final prev = result;
      result = await _ConversionResult.fromFile(outputImageFile, inputSize: await imageFile.length());
      prev?.dispose();
      if (mounted) {
        setState(() {});
      }
    }
  }
}

class _ConversionResult {
  _ConversionResult({required this.file, required this.image, required this.inputSize, required this.outputSize});
  final File file;
  final ui.Image image;
  final int inputSize;
  final int outputSize;

  static Future<_ConversionResult> fromFile(File file, {required int inputSize}) async {
    final comp = Completer<_ConversionResult>();
    ui.decodeImageFromList(await file.readAsBytes(), (r) async {
      comp.complete(_ConversionResult(file: file, image: r, inputSize: inputSize, outputSize: await file.length()));
    });
    return await comp.future;
  }

  void dispose() {
    image.dispose();
  }
}
