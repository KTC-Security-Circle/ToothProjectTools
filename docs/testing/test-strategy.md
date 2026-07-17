# テスト方針

## Unit test

単一の処理を検証する。
実カメラ、Window、プロジェクタ表示を使わない。

対象:

- Resolver
- Validator
- DecodeService
- Calibration loader
- Command Mapper
- Result Mapper

## Integration test

複数の処理を組み合わせて検証する。
offlineで完結するものだけをCI対象にする。

対象:

- Handler + Service
- Dispatch + Handler
- Command Mapper + Dispatch

## Serve app smoke

`tooth-backend serve` を起動する。
stdinへJSON Linesを送る。
stdoutのreturnとeventを検証する。
実カメラ、Window、プロジェクタ表示を使わないcaseをCI対象にする。

## Manual hardware test

実機resourceを使う。
CIでは実行しない。

対象:

- real camera
- real projector
- manual scan
- stream
- capture

## CI対象

- Unit test
- Offline integration test
- Offline serve app smoke

## CI非対象

- real camera
- real projector
- manual scan
