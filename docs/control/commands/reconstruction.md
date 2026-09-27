# 3D再構成command

## reconstruct_validate
保存済みdecode resultとstereo calibrationを同期検証する。Camera、Projector、Windowへ依存しない。

```json
{"id":"1","cmd":"reconstruct_validate","decode_dir":"./data/decode/session","calibration_file":"./data/calib/stereo.yml"}
```

## reconstruct_point_cloud
left camera座標系・単位mmのASCII XYZ PLYを同期生成する。projector座標はexact一致、`max_epipolar_error_px`のdefaultは2.0 pxである。正のdepthだけを採用する。`min_depth_mm`と`max_depth_mm`は任意で、defaultでは無効である。

```json
{"id":"2","cmd":"reconstruct_point_cloud","decode_dir":"./data/decode/session","calibration_file":"./data/calib/stereo.yml","output_file":"./data/reconstruction/cloud.ply"}
```

`overwrite`のdefaultはfalse。event、status、cancel、background workerはない。相対pathはprocess working directory基準で解決する。主なerror codeは`decode_dir_not_found`、`decode_result_invalid`、`calibration_file_not_found`、`calibration_file_invalid`、`image_size_mismatch`、`insufficient_valid_correspondence`、`triangulation_failed`、`output_file_exists`、`file_write_failed`である。

高レベル `stereo_scan` はこの同じ `ReconstructionService` を呼び出し、defaultで `<output_dir>/decode` と `data/calib/stereo.yml` を入力、`<output_dir>/cloud.ply` を出力にする。再構成ロジックやPLY writerはFacade内に重複実装しない。
