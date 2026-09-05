# Calibration workflowの責務境界

## 目的

Calibrationは画像を保存して数値を出力するだけの処理ではない。撮影姿勢の偏りや不安定なframeをsolverへ渡すと、`K`、`D`、`R`、`T`、`Q`の再現性が失われ、後段の三角測量へ誤差が伝播する。

そのため、frame観測、撮影成立条件、captureの副作用、solver、Calibration file保存を分離する。

```text
Camera frame
  -> Checkerboard observation
  -> Guidance / blur / stability
  -> CalibrationSession state
  -> CaptureService
  -> Calibrator or StereoCalibrator
  -> CalibrationValidator
  -> atomic file writer
  -> ReconstructionService validation
```

## State Pattern

`CalibrationSession`は撮影workflowの状態を1つだけ保持する。`waiting_for_board`、`guiding`、`stabilizing`、`ready_to_capture`、`captured`、`next_pose`、`solving`、`validating`、`completed`、`failed`、`cancelled`を状態として扱う。

State Patternを使う理由は、`is_ready`、`is_stable`、`is_captured`を個別のboolで管理すると、未検出なのに撮影可能などの矛盾を作れるためである。状態遷移は判定ロジックに限定し、Cameraやfilesystemの副作用は上位workflowが状態を見て実行する。

## Strategy Pattern

MonoとStereoではcheckerboard観測を共有するが、capture成立条件は異なる。Monoは1 cameraの観測、Stereoは左右cameraの観測と同一pair性を必要とする。この差分はcapture strategyへ閉じ込め、checkerboard検出やstability判定を複製しない。

Auto/Manualの差分も、成立条件ではなく「成立後に自動保存するか、キー入力を待つか」に限定する。`capture`の意味はどのstrategyでも「成立済みframeを指定pathへ保存する」で統一する。

## 副作用境界

以下は純粋なdomain判定であり、Camera、Window、filesystemへ触れない。

- checkerboard検出とcorner refinement
- board位置、サイズ、rotation、perspectiveの評価
- blur判定
- 前frameとの差分によるstability判定
- near/middle/far分類
- CalibrationSessionの状態遷移

以下は副作用を持つ境界である。

- Camera open/closeとframe取得: `CameraService`、`CaptureService`
- Preview: `WindowService`
- Calibration画像保存: capture writer
- Calibration結果保存: atomic file writer
- JSON Lines response/event: Control adapter

## 画像上の距離区分

Calibration前はboardの正確な3D距離を保証できないため、cm値を表示しない。near/middle/farは画像占有率から求め、board幅・高さと合わせて撮影計画の分布を作る。

## 3D復元との契約

Calibration結果は、ファイルが生成されたことだけで成功とはしない。`K`、`D`、`R`、`T`、`Q`、RMS、image sizeを検証し、保存後は実際のReconstruction validationへ通す。失敗時は既存の成功済みfileを壊さず、temporary fileも削除する。
