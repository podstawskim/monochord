MONOCHORD: 

1. konieczne dodanie flagi "-lm" podczas kompilacji programu
2. przesłanie raportu z monochodrdu do gniazda UDP jest możliwe tylko jeśli po spacji, lub tabulacji(opcjonalnie ":") doda się jakikolwiek ciąg znaków. Zastosowanie takiego rozwiązania zmusiło mnie wykorzystanie funkcji strtot() w get_data()
3. jeśli parametry programu ustawiane są pojedynczo najlepiej ustawić najpierw PID programu 
odbierającego oraz sygnał RT, a dopiero potem period

REJESTRATOR:

1. komendy przekazywane są jako jedna liczba, to znaczy: jeśli chce się zacząć rejestrację (1) i
używać identyfikacji źródeł to komenda przesłana przez sygnał sterujący musi wyglądać np: 104. Kolejność opcji po podaniu 1 nie ma znaczenia
2. rejestrator po uruchomieniu pokazuje swój PID
3. aby korzystać z stdout wymagane jest podanie po fladze -t znaku "-"
