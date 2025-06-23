# OurScheme-Project
原神大學Programming Language課程使用

## 建議執行測試方法(PowerShell)
1. 將當前位置導向程式碼所在的資料夾
   ```
   cd C:\Users\york\OneDrive\Desktop\PL
   ```
2. 先將想測試的 input 內容寫成 input.txt 檔後，以下方指令來讓對應的 test.exe 去執行測試
   ```
   Get-Content input.txt | .\test.exe
   ```
3. 接著就會在終端上看見所有 output 了 (建議可用 WinMerge 來對答案)
