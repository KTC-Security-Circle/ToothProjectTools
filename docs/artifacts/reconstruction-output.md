# 再構成出力

reconstruction output は3D再構成で生成するpoint cloudである。
`reconstruct_point_cloud` は現在未実装である。

## 構成

```text
reconstruction/
  cloud.ply
```

## 生成するcommand

- `reconstruct_point_cloud`

## 読むcommand

なし。

## 必須field

| field | 説明 |
| --- | --- |
| `cloud.ply` | 3D point cloud。 |

## 任意field

| field | 説明 |
| --- | --- |
| `metadata.json` | 生成条件。現在は未実装である。 |

## file format

`cloud.ply` はPLYである。
頂点には少なくともXYZ座標を含める。
