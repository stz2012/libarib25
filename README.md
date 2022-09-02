# libarib25 (STZ版)

[![Build Status](https://github.com/stz2012/libarib25/actions/workflows/build.yml/badge.svg)](https://github.com/stz2012/libarib25/actions/workflows/build.yml)
[![Build Status](https://ci.appveyor.com/api/projects/status/github/stz2012/libarib25?branch=master&svg=true)](https://ci.appveyor.com/project/stz2012/libarib25)
[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)


## STZ版について
オリジナルのコードは、茂木 和洋 (MOGI, Kazuhiro) 氏によって「ARIB STD-B25 仕様確認テストプログラムソースコード」という名称で Windows 向けに開発され VisualStudio（Visual C++）をターゲットに開発されました。

後に、有志の手により `libarib25` として Linux でビルド可能なパッチが書かれ、同時期に複数の無名の開発者により 2ch 等で公開されましたが、その後時間の経過とともに機能改善が散逸的に行われていったため、パッチをかき集めないとビルドするのが難しい状態となっていました。

このソフトウェアは、これらの無名の開発者の成果を Github のリポジトリにまとめ上げたものがベースとなっています。

Linux に移植された `libarib25` ですが、元々 Windows 上の VisualStudio で書かれたコードであるため、Windows でも同一のコードベースにより DLL ファイル等をビルドできるようにするため、ビルドシステムを CMake に移行し、再度 Windows をサポート。macOS X のサポートや、Raspberry Pi 等の ARM 環境のサポート。オプションで Intel の AVX2, ARM の NEON 等 SIMD による MULTI2 復号処理に対応するなどより幅広い環境をサポートするよう改良が加えられています。

また、ライセンスが不明瞭であったことによる不便さを解消する目的でコミュニティの同意のもと [Apache License 2.0](LICENSE) に移行していますが、オリジナルの作者である 茂木 和洋 氏はじめ無名の有志の方々の著作権は消失していないことにご留意下さい。詳しくは、[NOTICE](NOTICE) ファイルを参照下さい。また [免責事項](#免責事項) もあわせて参照してください。

## ビルド方法

### Linux

#### 依存関係のインストール
事前にビルドに必要な開発環境及びライブラリをインストールします。

##### <u>Ubuntu</u>
```sh
sudo apt-get install build-essential pkg-config git cmake libpcsclite-dev
```

##### <u>Redhat</u>
```sh
sudo dnf install "@Development tools" gcc-c++ cmake pcsc-lite-devel
```

#### ソースコードの取得とビルド
ソースコードを GitHub から取得します。
```sj
git clone https://github.com/stz2012/libarib25.git
cd libarib25
```

ビルド用のディレクトリを作成し`cmake` コマンドを実行しビルドに必要なコンフィグファイルを生成します。この際、`cmake` コマンドにオプションとしてビルドオプションを指定することができます。詳細は [ビルドオプション](#ビルドオプション) の項を参照してください。何も指定しない場合デフォルトの設定でコンフィグファイルを生成します。

`cmake` コマンドを実行するフォルダは build 以外の任意の名前で構いません。ビルドオプションを変えて異なるバイナリを生成したい場合等には、別の名前のフォルダを作っておくと異なる設定でのビルドを互いに影響を及ぼすことなく独立して管理できます。

```sh
mkdir build
cd build

cmake ..
```

正常にコンフィグファイルが生成されると、デフォルトでは GNU make 用の Makefile が同時に生成されます。下記のように `make` コマンドを実行するとビルドが始まります。

```sh
make
```

ビルドが正常に完了すると `libarib25.so` と `b25` コマンドが生成されるので、システムにこれらのライブラリや実行ファイル、ヘッダーファイル一式をインストールするには、下記のように管理者権限で `make install` を実行してください。

```sh
sudo make install
```

下記のように `b25` コマンドを実行しヘルプメッセージが表示されれば正常にインストールが完了しています。

```sh
b25
```
```
b25 - ARIB STD-B25 test program version stz-0.2.5 (v0.2.5-20190204-20-gbfc502d)
  built with GNU 9.4.0 on Linux-5.10.76-linuxkit
usage: b25 [options] src.m2t dst.m2t [more pair ..]
options:
  -r round (integer, default=4)
  -s strip
     0: keep null(padding) stream (default)
     1: strip null stream
  -m EMM
     0: ignore EMM (default)
     1: send EMM to B-CAS card
  -p power_on_control_info
     0: do nothing additionally
     1: show B-CAS EMM receiving request (default)
  -v verbose
     0: silent
     1: show processing status (default)
  -V, --version
     show version
  -h, --help
     show this help message
```

### Windows
### VisualStudio
#### 依存関係のインストール
##### <u>VisualStudio 2019 以上の場合</u>
* [VisualStudio 2022](https://visualstudio.microsoft.com/ja/downloads/)  
ワークロードから 「C++ によるデスクトップ開発」を選択してインストールします。

##### <u>VisualStudio 2017 以前の場合</u>
VisualStudio 2017 以前では、VisualStudio の他に Git for Windows を手動でインストール必要があります。

* VisualStudio 2017  
ワークロードから 「C++ によるデスクトップ開発」を選択してインストールします。

* [Git for Windows](https://gitforwindows.org/)  
上記リンクより Git for Windows をダウンロードしインストールします。

##### <u>VisualStudio 2015 以前の場合</u>
VisualStudio 2015 以前の場合 CMake が統合されていないため、Git for Windows に加えて CMake もインストールする必要があります。
* VisualStudio  
ワークロードから 「C++ によるデスクトップ開発」を選択してインストールします。
* [Git for Windows](https://gitforwindows.org/)  
上記リンクより Git for Windows をダウンロードしインストールします。
* [CMake](https://cmake.org/download/)  
上記リンクより最新の Windows 版 (msi) パッケージをダウンロードしインストールします。

#### ソースコードの取得
##### <u>VisualStudio 2019 以上の場合</u>
VisualStudio を起動後のスタート画面右側「開始する」から「リポジトリのクローン」を選択し、リポジトリの場所に Github のリポジトリ URL を入力するとソースコードを取得することができます。

##### <u>VisualStudio 2017 以前の場合</u>
Git for Windows の bash シェルを起動し、Github のリポジトリを手動でクローンします。

```sh
mkdir -p ~/source/repos/
cd ~/source/repos

git clone https://github.com/stz2012/libarib25.git
```

#### ソースコードのビルド
##### <u>VisualStudio 2019 以上の場合</u>
VisualStudio ビルトイン機能でクローンした場合は、右側のソリューションエクスプローラから「フォルダービュー」をダブルクリックするとプロジェクトを開くことができます。VisualStudio に統合された CMake により自動でビルドに必要な設定ファイルが生成されるので「ビルド」メニューより「全てビルド」を選択することでビルドすることができます。

##### <u>VisualStudio 2017</u>
VisualStudio 2017 の場合は、VisualStudio 起動後「ファイル」メニューから「開く」→「フォルダ」を選択し、ソースコードの取得でクローンしたフォルダを選択し開きます。VisualStudio 2017 では CMake が統合されているため、フォルダを開いた後 VisualStudio 2019 以上と同様に CMake により自動で設定ファイルが構成されます。「ビルド」メニューより「全てビルド」を選択しビルドしてください。

##### <u>VisualStudio 2015 以前</u>
VisualStudio 2015 以前の場合は、CMake が統合されておらず手動で VisualStudio のソリューションファイルを生成する必要があります。

CMake GUI を起動します

1. 「where is the source code」欄右側の「Browse source...」ボタンを選択し、ソースコードの取得でクローンしたパスを指定します
2. 「where to build the binaries」欄にソリューションファイルの生成するディレクトリを指定します  
1 の手順で指定したディレクトリの配下に build 等の名前でディレクトリを生成し指定することを推奨しますが、1 で指定したパスと同じでも構いません。
3. 「Configure」ボタンを押す  
「CMakeSetup」画面が表示されるので、「Specify the generator for this project」欄より該当する VisualStudio のバージョンを選択し「Finish」ボタンを押します。  
正常に完了すると CMake GUI の下部ログ画面に「Configure done」と表示されます。
4. 「Generate」ボタンを押して VisualStudio のソリューションファイルを生成します。

手順 2 で指定したディレクトリにソリューションファイル (.sln) ファイルが生成されているので、VisualStudio で開きます。「ビルド」メニューより「ソリューションのビルド」を選択することでビルドすることができます。

## ビルドオプション

### コンパイラの指定
既定では CMake によりシステムにインストールされているコンパイラが自動で検出されます。クロスコンパイルをしたい場合や、何らかの理由で別のコンパイラを使用したい場合には下記のように指定することで使用するコンパイラを変えることができます。

例) コンパイラとして `clang` を使用する場合

```sh
cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang ..
```

### PC/SC ライブラリの指定
既定では各 OS における PC/SC の API やライブラリを自動で検出しリンクします。Linux では `libpcsclite`、Windows では `WinSCard` (winscard.dll)、macOS X では `winscard` にリンクされます。

| Option            | Default     |
| ----------------- | ----------- |
| WITH_PCSC_PACKAGE | libpcsclite |
| WITH_PCSC_LIBRARY |             |

`WITH_PCSC_PACKAGE` に pkg-config が認識するパッケージ名を入力すると、自動でインクルードパスの設定及びリンクを行うように構成します。このオプションを使用することで、PC/SC 互換のサードパーティライブラリとリンクすることができます。ただし、この方法は pkg-config 用設定ファイル（拡張子 .pc）のファイルが提供されていること、pkg-config のインクルードパス以外に展開サれている場合は環境変数 `PKG_CONFIG_PATH` を事前に通しておく必要があります。

例) `libpcscmod` という名前の PC/SC 互換ライブラリとリンクする場合。

```sh
PKG_CONFIG_PATH=/usr/local/lib/pkgconfig  cmake -D WITH_PCSC_PACKAGE=libpcscmod ..
```

サードパーティのライブラリが pkg-config 形式の設定ファイルを提供していない場合は、ライブラリ名を入力することでシステムから探索し自動でリンクするように構成します。ただし、事前にシステムに libpcsclite パッケージがインストールされていると優先的に `WITH_PCSC_PACKAGE` で指定された pkg-config 形式のライブラリ設定が読み込まれるためこれを無効にする必要があります。

例) `pkg-config` がデフォルトで検出する `libpcsclite` を無効化し `libpcscmod` にリンクする場合。
```
cmake -DWITH_PCSC_PACKAGE=NO -DWITH_PCSC_LIBRARY=pcscmod ..
```

一般的なディレクトリ以外にインストールされたライブラリの場合自動で検出されます。クロスコンパイル等で特殊なディレクトリを参照させたい場合は `CMAKE_FIND_ROOT_PATH` を指定する必要があるかもしれません。


### オプション機能
#### UNICODE 対応（Windows のみ）
デフォルトで ON になっています。コマンドラインで日本語を含むファイルパスを指定する場合、UNICODE 対応を ON の状態でビルドする必要があります。無効化するメリットはありませんが、何らかの理由で無効化したい場合は `-DUSE_UNICODE=OFF` と指定してビルドすることができます。

| Option      | Default |
| ----------- | ------- |
| USE_UNICODE | ON      |


#### SIMD 対応
それぞれ `-DUSE_AVX2=ON`, `-DUSE_NEON=ON` とすることで有効化できます。NEON については、Windows 以外の ARM CPU (現時点では リトルエンディアンのみ)に対応。

| Option      | Default |                 |
| ----------- | ------- |-----------------|
| USE_AVX2    | OFF     |                 |
| USE_NEON    | OFF     | Windows は未対応 |

ARM CPU は、バイエンディアンですが Raspberry Pi 等の既定ではリトルエンディアンとなっているはずなので多くの場合問題にはなりません。ビッグエンディアン環境で NEON を有効化しビルドしようとするとエラーになります。

## 免責事項
本ソフトウェアは現状有姿で提供され、明示であるか暗黙であるかを問わず、いかなる保証も致しません。ここでいう保証とは、商品性、特定の目的への適合性、および権利非侵害についての保証も含みますが、それに限定されるものではありません。作者または著作権者は、契約行為、不法行為、またはそれ以外であろうと、ソフトウェアに起因または関連し、あるいはソフトウェアの使用またはその他の扱いによって生じる一切の請求、損害、その他の義務について何らの責任も負わないものとします。

## ライセンス
このソフトウェアは [Apache License 2.0](LICENSE) の下で提供されます。

## 参考
* 一次配布元  
㋲製作所 - http://www.marumo.ne.jp/db2012_2.htm#13  
README（オリジナル）- [README](README.org)
