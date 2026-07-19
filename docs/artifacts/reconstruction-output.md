# 再構成出力

`reconstruct_point_cloud`はASCII PLY 1.0を生成する。vertexはleft camera座標系、単位mmのXYZを持つ。color、normal、face、debug画像、depth画像、YAMLはMVPでは生成しない。既存fileは`overwrite=true`の明示がない限り置換しない。
