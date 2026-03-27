# Pre-commit 使用说明

## 安装

在终端中运行以下命令来安装 pre-commit：

```sh
sudo apt install pre-commit
```

## 安装 Git Hook 脚本

在项目根目录运行以下命令来安装 pre-commit 的 Git hook 脚本：

```sh
pre-commit install
```

## 使用

每次提交代码时，pre-commit 都会自动运行。如果你想手动运行 pre-commit，可以使用以下命令：

```sh
pre-commit run --all-files
