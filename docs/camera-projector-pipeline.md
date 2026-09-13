# Camera–Projector復元

このfeatureは、single CameraでProjectorのGray Codeを撮影し、Camera pixelから
Projector logical coordinate (480x270)をdecodeして3D復元する経路を追加する。

## 座標と校正

Projectorは画像を撮影しない逆向きCameraとして扱う。校正ファイルの
`R_camera_to_projector` と `T_camera_to_projector` は次を表す。

```text
X_projector = R_camera_to_projector * X_camera + T_camera_to_projector
```

三角測量では歪みを除去したCamera/Projector座標と、
`P_camera=[I|0]`、`P_projector=[R|T]`を使用する。無限値、後方点、深度範囲外、
reprojection error超過点はPLYへ出力しない。

## 同期

Projector canvasに余白がある場合、active Gray Code領域外の8x8 markerを表示する。
markerはpattern indexを符号化せず、index parityに応じて黒白を交互に切り替え、
光学的な切替edgeの検出にのみ使う。余白がない場合はmarkerを表示せず、active
patternを破壊しない。

Camera frame timestampは`VideoCapture::read()`成功直後にhostで取得した
`steady_clock`時刻であり、sensor露光時刻またはV4L2 hardware timestampではない。
Photodiodeを接続する場合も、device timestampを直接比較せず、host受信時刻へ変換する。

## 校正dataset

Boardは10x7 internal corners、square sizeは実物に合わせたmm値を使用する。
各poseはReference Before、38枚の既存Gray Code sequence、Reference Afterの順で保存する。
Before/After cornerの平均・最大移動量が閾値を超えるposeは破棄する。subpixel corner
に対するProjector座標は周囲5x5のvalid decoded mapから中央値で推定する。

27姿勢はnear/middle/far各9姿勢の`defaultPosePlan()`で列挙する。実機未接続環境では
このdataset取得と光学同期の実測は未確認であり、4.10環境でのbuild確認も別途必要である。
