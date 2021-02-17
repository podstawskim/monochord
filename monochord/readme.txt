POPRAWA REJESTRATORA:

1. PID programu pojawia się w pierwszej linijce pod napisem "REJESTRATOR" po uruchomieniu programu.
2. Interpretacja sygnału jest teraz zgodna ze specyfikacją.
3. Problem z raportowaniem zawsze 1 przy START STOP wynikał z błędnej interpretacji sposobu przesyłania i odbierania komendy sterującej. 
4. Po otrzymaniu sygnału sterującego pokazywane rejestrator wyświetla: 
-że otrzymano sygnał z komendą, jaki RT signal go wysłał, PID procesu wysyłającego RT signal oraz komendę przez niego wysłaną (screen1), 
-jeśli komenda zaczyna zaczyna rejestrację pojawia się "Registration: OPEN" w przeciwnym wypadku "Registration: CLOSED"
5. Po otrzymaniu sygnału z danymi program wyświetla:
-że otrzymano sygnał z komendą, jaki RT signal go wysłał, PID procesu wysyłającego RT signal oraz komendę przez niego wysłaną (screen2), 
- następnie powinno być "inserting into text file/stdout" i w zależności od tego czy używam stdout czy nie to pod tym napisem pojawi się linijka z odebranymi danymi oraz dodatkowymi opcjami
- jeśli format binarny jest używany program wyświetli "inserting data into binary file"

PS wszystkie problemy wymienione w raporcie były spowodowane moją złą interpretacją sposobu przesyłania i odbierania komend, teraz powinno się wszystko zgadzać.
