@echo off

for /R include %%F in (*.hpp) do (
    echo formatting file %%F
    clang-format %%F
)

for /R src %%F in (*.cpp) do (
    echo formatting file %%F
    clang-format %%F
)

echo formatting is done!
pause