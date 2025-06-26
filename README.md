# OurScheme-Project
Using for China Yuanshen University Programming Language class.

Chen You 2025

## OurScheme Interpreter
- OurScheme is a simple Scheme interpreter implemented in C++. It is designed to read, parse, and evaluate S-expressions, providing a basic environment for executing Scheme-like code.

## Tools
- Visual Studio Code
- WinMerge
- Dev-C++

## Recommended test execution method(PowerShell)
1. 將當前位置導向程式碼所在的資料夾
   ```
   cd C:\Users\york\OneDrive\Desktop\PL
   ```
2. 先將想測試的 input 內容寫成 input.txt 檔後，以下方指令來讓對應的 test.exe 去執行測試
   ```
   Get-Content input.txt | .\test.exe
   ```
3. 接著就會在終端上看見所有 output 了，又或是可用下方指令來把 output 內容都寫入一個新的 result.txt 檔方便於對答案 (建議可用 WinMerge 來對答案)
   ```
   Get-Content input.txt | .\test.exe > result.txt
   ```
4. 在此也提供一種直接對答案的指令，若檔案內容完全相同的話，終端上就不會有任何輸出內容
   ```
   Compare-Object (Get-Content result.txt) (Get-Content answer.txt) | Select-Object
   ```
## Small writing experience
- 建議前期的各個 function 先不要寫得太高度相互關聯性，能把工作都分清楚給各個 function 獨立運作最佳。
- 不要輕視任何一個 Error Message 處理上的邏輯設計，一個沒想好就直接寫，很容易會在除錯後面的題目時碰壁(尤其是可能會卡隱測)。
- 體感難度為 project 3 > 1 > 2 > 4，若在 3 的後面幾題被 Error 卡住，可直接跳 4 也無妨。
- 個人認為 Error Handling 難度更大於各個功能實現，可以的話是能先把各個功能都實作完善後，最後再針對 Error Handling 的部份去做完整處理（邊寫邊做的風險是很容易變成在看著 input 內容做爆破）。
- 個人習慣是去挖出每一題的明測一改到程式碼的運作都符合正確答案後，再一次去系統過每一題，把每一題一路車過去超爽。
- 理論上 Error Handling 的各種形式在越後面的題目就都會測到了，所以不至於要擔心太前面就卡在隱藏的問題，有可能後面的測資都改到好就能一次解決前面卡隱藏的問題了。
- 寫到最後一刻沒能把每一題都過完實屬遺憾，要是還有時間的話，確實會考慮把 porject3 的 Error Handling 全部打掉重寫，也是整個程式碼中的最大敗筆。
- 建議不要用 AI 強改功能出來，高機率會被改爛（我 project1 的 Error Handling 就是如此，所以後來全部打掉自己重寫ㄏㄏ）。
## Conclusion
- 不想努力就抄看看吧。
