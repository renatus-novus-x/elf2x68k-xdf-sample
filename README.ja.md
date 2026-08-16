# elf2x68k XDF/HDF Sample

[English](README.md) | [日本語](README.ja.md)

<p align="center">
  <img src="images/teapot.png" alt="X68000 / Human68k 上で動作するティーポットのワイヤフレーム" width="640">
</p>

<p align="center">
  <strong><a href="https://uraraworks.github.io/WebX68k/?cpu=100&ram=12&hdd=https%3A%2F%2Fraw.githubusercontent.com%2Frenatus-novus-x%2Felf2x68k-xdf-sample%2Frefs%2Fheads%2Fmain%2Fimages%2Fteapot.zip&run=1">▶ WebX68k ですぐにティーポットのサンプルを実行</a></strong>
</p>

[elf2x68k](https://github.com/yunkya2/elf2x68k) で X68000 用プログラムをビルドし、起動可能な Human68k のディスクイメージへ格納するための小さな Makefile サンプル集です。

このリポジトリでは、次の2種類のシンプルな構成を用意しています。

- `samples/hello` — 最小プログラムを起動可能な **XDF フロッピーイメージ**へ格納
- `samples/teapot` — OBJ ワイヤフレームビューアとモデルを起動可能な **SASI HDF ハードディスクイメージ**へ格納

どちらのサンプルも、公式 Human68k 3.02 配布物の取得、[XDFtool](https://github.com/yunkya2/x68kmisc/tree/main/xdftool) によるディスクイメージの再構築、`AUTOEXEC.BAT` の変更まで Makefile から自動的に行い、Human68k 起動後にサンプルが自動実行されるようにしています。

## リポジトリ構成

```text
.
├── README.md
├── README.ja.md
├── images/
│   ├── teapot.png
│   └── teapot.zip
└── samples/
    ├── hello/
    │   ├── Makefile
    │   └── hello.c
    └── teapot/
        ├── Makefile
        ├── model.obj
        └── teapot.c
```

## WebX68k ですぐに実行

ローカルにエミュレータやビルド環境を用意しなくても、生成済みのティーポットサンプルを [WebX68k](https://uraraworks.github.io/WebX68k/) から直接起動できます。

**[▶ teapot.zip を WebX68k で実行](https://uraraworks.github.io/WebX68k/?cpu=100&ram=12&hdd=https%3A%2F%2Fraw.githubusercontent.com%2Frenatus-novus-x%2Felf2x68k-xdf-sample%2Frefs%2Fheads%2Fmain%2Fimages%2Fteapot.zip&run=1)**

このリンクは `images/teapot.zip` を HDD イメージとして読み込み、そのまま自動起動します。

## 必要な環境

Makefile は Ubuntu 24.04 LTS などの Linux 環境を想定しています。WSL2 上の Ubuntu 24.04 LTS でも利用できます。

### elf2x68k

あらかじめ [elf2x68k](https://github.com/yunkya2/elf2x68k) をインストールし、クロスコンパイラへ `PATH` が通っていることを確認してください。

```sh
m68k-xelf-gcc --version
```

### Ubuntu 側のツール

必要なパッケージをインストールします。

```sh
sudo apt update
sudo apt install make curl unar python3
```

`unar` は公式 Human68k の LZH アーカイブを展開するために使用します。アーカイブ内の日本語ファイル名は Shift_JIS として解釈し、ファイル本文の文字コード変換は行いません。

## Sample 1: 起動可能な XDF フロッピー

`samples/hello` は最小構成のサンプルです。

```sh
cd samples/hello
make
```

以下が生成されます。

```text
hello.x
hello.xdf
hello.zip
```

Makefile は次の処理を行います。

1. `hello.c` を `m68k-xelf-gcc` でコンパイル
2. 公式 Human68k 3.02 アーカイブをダウンロード
3. `HUMAN302.XDF` を展開
4. `xdftool.py` で Human68k フロッピー内のファイルを展開
5. `hello.x` をディスクへ追加
6. `AUTOEXEC.BAT` の末尾へ `hello.x` を追加
7. 1232 KB の起動可能な X68000 XDF を再生成
8. 生成した XDF と Human68k の許諾条件文書を `hello.zip` に格納

起動の流れは次のようになります。

```text
X68000 起動
    ↓
Human68k
    ↓
AUTOEXEC.BAT
    ↓
hello.x
```

生成された XDF の内容は、

```sh
make check-xdf
```

で確認できます。

## Sample 2: 起動可能な SASI HDF

`samples/teapot` は、フロッピーより大きなプログラムやデータを扱うための HDF サンプルです。

```sh
cd samples/teapot
make
```

以下が生成されます。

```text
teapot.x
teapot.hdf
teapot.zip
```

`teapot.hdf` は XDFtool の `/h10` 形式を使用して作成する **10 MB の SASI Human68k ハードディスクイメージ**です。

HDF には、

```text
teapot.x
model.obj
```

の両方が格納されます。

Makefile は `AUTOEXEC.BAT` に、

```text
teapot.x model.obj
```

を追加するため、Human68k の起動後にティーポットが自動的に表示されます。

生成された HDF の内容は、

```sh
make check-hdf
```

で確認できます。

## ティーポットサンプル

ティーポットサンプルは、Human68k 上で動作する小さな Wavefront OBJ ワイヤフレームビューアです。

X68000 のグラフィック画面を IOCS で、

```c
_iocs_crtmod(0x0e);
_iocs_g_clr_on();
```

と初期化し、モデルの各辺を、

```c
_iocs_line(&param);
```

で描画します。

OBJ 全体の範囲から中心位置と倍率を自動計算し、モデルがグラフィック画面内に収まるように表示します。実行時に OpenGL や MiniGL は必要なく、描画は X68000 の IOCS を直接利用しています。

OBJ の読み込みやワイヤフレーム描画の考え方は [miniglut-x68k](https://github.com/renatus-novus-x/miniglut-x68k) と関連していますが、このリポジトリでは elf2x68k とディスクイメージ生成のサンプルとして小さくまとめています。

## ディスクイメージ生成の流れ

どちらのサンプルも基本的には同じ処理を行います。

```text
C ソース
   │
   │ m68k-xelf-gcc
   ▼
Human68k .x 実行ファイル

HUMN302I.LZH
   │
   │ unar -e shift_jis
   ▼
HUMAN302.XDF
   │
   │ xdftool.py x
   ▼
Human68k のファイル一式
   │
   ├── アプリケーションを追加
   ├── AUTOEXEC.BAT を変更
   │
   └── xdftool.py c
          │
          ├── XDF フロッピーイメージ
          └── SASI HDF ハードディスクイメージ
```

Human68k 自体をサンプルディレクトリへ直接収録するのではなく、必要になった時点で Makefile が公式 Human68k 3.02 配布物を取得します。

## Human68k の許諾条件について

Makefile は X68000 LIBRARY の公式配布先から Human68k 3.02 を取得します。

```text
http://retropc.net/x68000/software/sharp/human302/HUMN302I.LZH
```

元のアーカイブに含まれる許諾条件文書は、本文を文字コード変換せずバイト単位でそのままコピーし、生成する配布用 ZIP に同梱します。

Human68k を含む生成済みディスクイメージを再配布する場合は、必ず元の許諾条件を確認してください。

## XDFtool

ディスクイメージの操作には `x68kmisc` に含まれる `xdftool.py` を使用します。

https://github.com/yunkya2/x68kmisc/tree/main/xdftool

Makefile が固定したリビジョンの `xdftool.py` を自動的にダウンロードするため、通常は XDFtool を別途インストールする必要はありません。

XDFtool は X68000 の標準的なフロッピーイメージだけでなく、SASI ハードディスクイメージの生成にも対応しています。

## クリーン

各サンプルディレクトリで、

```sh
make clean
```

を実行すると、生成した実行ファイルやディスクイメージ、ZIP を削除します。ダウンロード済みの補助ファイルは残します。

ダウンロード済みファイルや中間ファイルもすべて削除する場合は、

```sh
make distclean
```

を実行します。

## 参考

- [elf2x68k](https://github.com/yunkya2/elf2x68k)
- [elf2x68k-sample](https://github.com/yunkya2/elf2x68k-sample)
- [XDFtool / x68kmisc](https://github.com/yunkya2/x68kmisc/tree/main/xdftool)
- [WebX68k](https://uraraworks.github.io/WebX68k/)
- [Human68k 3.02](http://retropc.net/x68000/software/sharp/human302/)
