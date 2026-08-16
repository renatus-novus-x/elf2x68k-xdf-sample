# elf2x68k .xdf/.hdf Sample

[English](README.md) | [日本語](README.ja.md)

<p align="center">
  <img src="images/teapot.png" alt="X68000 / Human68k サンプル" width="640">
</p>

<p align="center">
  <strong><a href="https://uraraworks.github.io/WebX68k/?cpu=100&ram=12&hdd=https%3A%2F%2Fraw.githubusercontent.com%2Frenatus-novus-x%2Felf2x68k-xdf-sample%2Frefs%2Fheads%2Fmain%2Fimages%2Fteapot.zip&run=1">▶ teapot.zip を WebX68k ですぐ実行</a></strong>
</p>

[elf2x68k](https://github.com/yunkya2/elf2x68k) で X68000 用プログラムをビルドし、
起動可能な Human68k のディスクイメージへ格納するためのシンプルな Makefile サンプル集です。

このリポジトリでは主に次の2つの例を扱います。

- 起動可能な `.xdf` フロッピーディスクイメージの生成
- 起動可能な SASI `.hdf` ハードディスクイメージの生成

Makefile はビルド時に公式 Human68k 3.02 配布物を取得し、
[xdftool](https://github.com/yunkya2/x68kmisc/tree/main/xdftool) を使って
ディスクイメージを生成します。

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
        ├── teapot.c
        └── model.obj
```

## 必要な環境

Makefile は Ubuntu 24.04 LTS などの Linux 環境を想定しています。
WSL2 上の Ubuntu 24.04 LTS でも利用できます。

あらかじめ [elf2x68k](https://github.com/yunkya2/elf2x68k) をインストールし、
`m68k-xelf-gcc` に `PATH` が通っていることを確認してください。

必要なホスト側ツールは次のようにインストールできます。

```sh
sudo apt update
sudo apt install make curl unar python3
```

`unar` は公式 Human68k の LZH アーカイブを展開するために使用します。
アーカイブ内の日本語ファイル名は Shift_JIS として解釈します。

## .xdf サンプル

`samples/hello` は最小構成の .xdf 生成例です。

```sh
cd samples/hello
make
```

次のファイルが生成されます。

```text
hello.x
hello.xdf
hello.zip
```

`hello.xdf` は `hello.x` を含む起動可能な Human68k
フロッピーディスクイメージです。

Makefile は `AUTOEXEC.BAT` も更新し、Human68k 起動後に `hello.x` が
自動実行されるようにします。

生成したイメージの内容は次のコマンドで確認できます。

```sh
make check-xdf
```

## .hdf サンプル

`samples/teapot` は起動可能な SASI ハードディスクイメージを生成する例です。

```sh
cd samples/teapot
make
```

次のファイルが生成されます。

```text
teapot.x
teapot.hdf
teapot.zip
```

.hdf は xdftool の `/h10` 形式を使用し、10 MB の SASI ハードディスク
イメージとして生成されます。

生成したイメージの内容は次のコマンドで確認できます。

```sh
make check-hdf
```

## ビルド処理

どちらのサンプルも基本的には次の流れでディスクイメージを生成します。

```text
C ソース
   |
   | m68k-xelf-gcc
   v
.X 実行ファイル

HUMN302I.LZH
   |
   | unar -e shift_jis
   v
HUMAN302.XDF
   |
   | xdftool.py
   v
Human68k のファイル一式
   |
   | アプリケーションを追加
   | AUTOEXEC.BAT を更新
   v
起動可能な .xdf / .hdf
```

公式 Human68k 3.02 配布物は次の場所からビルド時に取得します。

```text
http://retropc.net/x68000/software/sharp/human302/HUMN302I.LZH
```

Human68k 自体はこのリポジトリには含めていません。

## Human68k の再配布条件

生成される .xdf/.hdf には Human68k のファイルが含まれます。

生成したイメージを再配布する場合は、公式 Human68k 3.02 配布物に含まれる
許諾条件を確認し、その条件に従ってください。

Makefile は元の許諾条件文書を保持し、生成する ZIP に同梱します。

## クリーン

生成した実行ファイルやディスクイメージを削除します。

```sh
make clean
```

ダウンロード済みの Human68k や中間ファイルも含めて削除する場合は、

```sh
make distclean
```

を実行します。

## 参考

- [elf2x68k](https://github.com/yunkya2/elf2x68k)
- [elf2x68k-sample](https://github.com/yunkya2/elf2x68k-sample)
- [xdftool / x68kmisc](https://github.com/yunkya2/x68kmisc/tree/main/xdftool)
- [Human68k 3.02](http://retropc.net/x68000/software/sharp/human302/)

## 謝辞

elf2x68k、xdftool の作者・コントリビューター、および X68000 コミュニティの皆様に感謝します。
