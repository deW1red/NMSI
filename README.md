# NMSI — Q-learning: lokalna realizacja trasy

W poprzednim ćwiczeniu robot dostawał trasę wyznaczoną algorytmem A\* i jechał po niej dzięki gotowemu, „ręcznie" napisanym regulatorowi. Tym razem **regulator zastąpisz agentem uczonym metodą Q-learning**: robot sam nauczy się, jak podążać za trasą, nie znając modelu swojej dynamiki ani żadnego wzoru na sterowanie.

## 1. Cel ćwiczenia

Węzeł `path_follower` odbiera trasę z tematu `/path` (nav_msgs/msg/Path) i ma ją „lokalnie realizować": w każdym kroku patrzy na punkt trasy leżący w odległości `head_radius` przed robotem i wybiera jedną z kilku dyskretnych akcji (zestaw prędkości) tak, aby dojechać do końca trasy bez kolizji i bez zjechania z niej.

Szkielet węzła `path_follower` (subskrypcje, publikacje, serwisy, reset robota, losowanie celów, odbiór trasy, wyszukiwanie punktu na trasie i timer) jest gotowy. Planowanie trasy zapewnia osobny węzeł `a_star`, którego implementację bierzesz z poprzedniego zadania. Lidar **nie jest** używany — trasa z A\* jest wolna od kolizji, a o kolizji dowiadujesz się z tematu `/collision`. Do napisania w pliku `nmsi_lab/src/path_follower.cpp` zostały funkcje oznaczone `TODO`:

| # | Funkcja | Zadanie |
|---|---------|---------|
| 1 | `discretize_state` | zamiana ciągłej obserwacji na dyskretny stan |
| 2 | `choose_action` | wybór akcji (strategia ε-zachłanna) |
| 3 | `execute_action` | zamiana akcji na prędkości i publikacja na `/cmd_vel` |
| 4 | `update_q_table` | aktualizacja Q-tablicy |
| 5 | `save_q_table` | zapis Q-tablicy do pliku |
| 6 | `load_q_table` | wczytanie Q-tablicy z pliku |
| 7 | `learning_loop` | jeden krok uczenia (wywoływany z timera) |

Do działania potrzebny jest węzeł `a_star` z poprzedniego zadania. Użyj własnej implementacji algorytmu, którą wtedy napisałeś. Jeśli nie udało Ci się jej ukończyć, poproś prowadzącego o kod — `a_star` nie jest gotowym elementem tego ćwiczenia.

Uruchomienie (dwa terminale):

```bash
# terminal 1
ros2 launch nmsi_lab simulation.launch.py world:=cave
# terminal 2
ros2 launch nmsi_lab robot_controller.launch.py
```

Budowanie po zmianach: `colcon build --packages-select nmsi_lab && source install/setup.bash` (w katalogu głównym przestrzeni roboczej).

Do nauki zacznij od świata `cave`. Gdy rozwiązanie będzie działać, sprawdź je również w `hallway` i `lines`.

## 2. Punktacja i zasady

### 2.1. Punktacja

| Zadanie | Opis | Punkty |
|---------|------|--------|
| 1 | Dyskretyzacja stanu (`discretize_state`) wraz z uzasadnieniem wyboru przedziałów | 2 |
| 2 | Wybór akcji ε-zachłanny i wykonanie akcji (`choose_action`, `execute_action`) | 1,5 |
| 3 | Aktualizacja Q-tablicy (`update_q_table`) | 1,5 |
| 4 | Zapis i wczytanie Q-tablicy (`save_q_table`, `load_q_table`) | 1 |
| 5 | Krok uczenia: nagrody, warunki końca epizodu, przechowywanie poprzedniego stanu (`learning_loop`) | 2 |
| 6 | Działająca polityka: po nauce robot w trybie „following" dojeżdża do celu bez kolizji w co najmniej 8 z 10 prób | 2 |
| Bonus | Muszę jeszcze wymyślić | +1 |

Termin oddania: **3 stycznia** (termin zapasowy: **10 stycznia** - 75% punktów). Kod źródłowy należy przesłać przez platformę **LeON** — wyłącznie pliki źródłowe (bez `build`, `install`, `log`) oraz **plik z nauczoną Q-tablicą**.

### 2.2. Samodzielność pracy

> **Zadanie trzeba wykonać samodzielnie.**
>
> Wolno korzystać z: własnej wiedzy, materiałów z wykładu (si8) i przygotowanych przez prowadzącego (w tym tej instrukcji i pseudokodu), dokumentacji ROS 2 oraz biblioteki standardowej C++.
>
> Nie wolno korzystać z: gotowych bibliotek i implementacji uczenia ze wzmocnieniem (np. Stable-Baselines, Gymnasium, PyTorch/TensorFlow), gotowych fragmentów kodu z internetu, kodu innych osób ani **kodu wygenerowanego przez modele językowe (LLM) i inne narzędzia AI**.
>
> Wykrycie pracy wykonanej niesamodzielnie lub z użyciem kodu wygenerowanego przez LLM skutkuje wyzerowaniem punktów za zadanie, utratą punktów bonusowych oraz odjęciem jednego punktu od pozostałych zadań. Prowadzący może poprosić o omówienie napisanego kodu.

W razie wątpliwości, czy coś jest dozwolone, **zapytaj prowadzącego zanim to wykorzystasz**.

## 3. Q-learning — teoria

(Szczegóły: wykład si8.)

### 3.1. Uczenie ze wzmocnieniem

Agent (robot) w kolejnych krokach $t$:

1. obserwuje stan $s_t$,
2. wybiera akcję $a_t$,
3. wykonuje ją, po czym otrzymuje nagrodę $r_t$ i obserwuje nowy stan $s_{t+1}$,
4. uczy się na podstawie krotki $(s_t, a_t, r_t, s_{t+1})$.

Formalnie jest to proces decyzyjny Markowa: zbiór stanów $S$, akcji $A$, funkcja nagrody i funkcja przejścia. Szukamy polityki $\pi: S \to A$ maksymalizującej zdyskontowaną sumę nagród

$$ \sum_{t=0}^{\infty} \gamma^t r_t, \qquad \gamma \in (0, 1]. $$

Parametr $\gamma$ określa, jak bardzo liczą się nagrody odległe w czasie. Zadanie ma charakter **epizodyczny**: epizod kończy się po osiągnięciu celu lub wystąpieniu porażki.

### 3.2. Funkcja Q

$Q(s, a)$ to oczekiwana, zdyskontowana suma nagród, jeśli w stanie $s$ wykonamy akcję $a$, a potem będziemy postępować optymalnie. Znając $Q$, polityka jest prosta: $\pi(s) = \arg\max_a Q(s, a)$. Dla małej liczby stanów i akcji $Q$ przechowujemy w tablicy (Q-tablicy).

### 3.3. Aktualizacja

Po każdym kroku poprawiamy jeden wpis tablicy:

$$ Q(s_t, a_t) \leftarrow Q(s_t, a_t) + \alpha \Big[ r_t + \gamma \max_{a'} Q(s_{t+1}, a') - Q(s_t, a_t) \Big] $$

- $\alpha \in (0, 1]$ — współczynnik uczenia,
- wyrażenie w nawiasie to błąd różnicy czasowej (rzeczywiste „r + γ·max Q" minus dotychczasowe oszacowanie),
- w **stanie terminalnym** nie ma następnego kroku, więc składnik $\gamma \max_{a'} Q(s_{t+1}, a')$ pomijamy (jest równy 0).

Q-learning jest algorytmem **off-policy** i **bezmodelowym**: aktualizacja używa $\max$ po akcjach, niezależnie od tego, jaką akcję agent faktycznie wybierze w następnym kroku, i nie wymaga znajomości funkcji przejścia (w odróżnieniu od programowania dynamicznego).

### 3.4. Eksploracja a eksploatacja

Gdy zawsze wybieramy najlepszą znaną akcję, agent może utknąć w złym rozwiązaniu. Strategia **ε-zachłanna**: z prawdopodobieństwem $\varepsilon$ wybierz akcję losową (eksploracja), z prawdopodobieństwem $1-\varepsilon$ najlepszą wg $Q$ (eksploatacja). Za mały $\varepsilon$ — polityka suboptymalna, za duży — wolna zbieżność. Często $\varepsilon$ zmniejsza się w trakcie uczenia. (Alternatywa: eksploracja Boltzmanna — losowanie akcji z prawdopodobieństwem zależnym od $Q$.)

## 4. Dyskretyzacja stanu, akcji i czasu

Q-tablica wymaga **skończonych, małych** zbiorów stanów i akcji. Robot działa w świecie ciągłym, więc trzeba zdecydować trzy rzeczy.

### 4.1. Stan

Funkcja `find_head_point()` zwraca punkt trasy leżący w odległości `head_radius` od robota (w układzie robota `base_link`; gdy robot jest bliżej celu — koniec trasy). Z tego punktu można policzyć wielkości opisujące sytuację robota względem trasy, np.:

- kąt do punktu: `atan2(y, x)` (czy trzeba skręcić w lewo, w prawo, czy jechać prosto),
- odległość do końca trasy.

Każdą wielkość można podzielić na kilka przedziałów (np. „mocno w lewo / lekko w lewo / prosto / lekko w prawo / mocno w prawo"). Jednym z możliwych sposobów reprezentacji stanu jest wektor indeksów przedziałów (`std::vector<int>`), który w obecnym szkielecie jest kluczem Q-tablicy. To tylko przykład — możesz wybrać inną reprezentację, jeśli lepiej pasuje do Twojego pomysłu. W takim przypadku dostosuj typ `DiscreteState` oraz typ klucza Q-tablicy.

Dobierz reprezentację i przedziały tak, aby liczba stanów była skończona i na tyle mała, by uczenie było wykonalne. Liczba stanów jest iloczynem liczby przedziałów dla poszczególnych wielkości; im więcej stanów, tym dłużej trwa nauka. Przy doborze granic weź pod uwagę fizyczne znaczenie obserwacji (np. `head_radius` = 0,4 m).

### 4.2. Akcje

Zbiór akcji to kilka gotowych par prędkości (liniowa, kątowa), np. „prosto", „łagodny skręt w lewo/prawo", „obrót w miejscu w lewo/prawo". `execute_action` zamienia numer akcji na `geometry_msgs/msg/Twist` i publikuje go na `/cmd_vel`. Stała `action_count` musi odpowiadać liczbie akcji.

### 4.3. Czas

**Ważne: w implementacji nie należy tworzyć pętli `for`/`while`, która wykonuje całe uczenie.** Q-learning opisuje kroki powtarzane w czasie, ale tutaj powtarzanie zapewnia timer ROS 2: co 100 ms wywołuje `follow_path()`, a ta w trybie jazdy po trasie wywołuje `learning_loop()`. **Jedno wywołanie `learning_loop()` wykonuje dokładnie jeden krok uczenia / jedną decyzję.** Funkcja zostaje wywołana ponownie przez timer; akcja (prędkość) działa do następnego wywołania.

Okres 100 ms jest już ustawiony w szkielecie. Zbyt krótki okres może oznaczać, że stan prawie się nie zmienia między decyzjami; zbyt długi może nie dać robotowi dość szybko zareagować na trasę.

## 5. ROS 2 — elementy potrzebne w tym ćwiczeniu

Wcześniej korzystaliśmy z węzłów, tematów i TF. Tu pojawiają się dodatkowo:

### 5.1. Serwisy

Serwis to komunikacja typu pytanie–odpowiedź (żądanie → odpowiedź), w przeciwieństwie do tematów, które są strumieniem danych. Typ `std_srvs/srv/Trigger` ma puste żądanie i odpowiedź z polami `success` i `message`.

```bash
ros2 service list                          # lista serwisów
ros2 service type /start_learning          # typ serwisu
ros2 service call /start_learning std_srvs/srv/Trigger "{}"
```

Zanim uruchomisz uczenie, możesz sprawdzić działanie serwisu bezpośrednio z CLI. Przy uruchomionej symulacji (terminal 1) wywołaj:

```bash
ros2 service call /random_reset std_srvs/srv/Trigger "{}"
```

Serwis przeniesie robota w inne, losowe wolne miejsce świata. To dobry sposób, żeby zobaczyć, jak robot i symulacja reagują na wywołanie serwisu, zanim uruchomisz własny kod.

W szkielecie są serwisy (zaimplementowane, nie zmieniaj ich):

| Serwis | Działanie |
|--------|-----------|
| `/start_learning` | tryb **uczenia**: robot jest resetowany w losowe miejsce, losowany jest cel, A\* wyznacza trasę, uruchamiany jest epizod; po jego końcu — następny |
| `/start_following` | tryb **jazdy po trasie**: zapis Q-tablicy do pliku, robot czeka na cel z `/goal_pose` i jedzie, zawsze wybierając najlepszą akcję |
| `/stop_path_follower` | zatrzymuje robota i czyści trasę |
| `/random_reset` | (Stage) przenosi robota w losowe wolne miejsce |

Program **startuje w trybie jazdy po trasie** (stan `WAITING_FOR_PATH`, `training == false`): nic się nie dzieje, dopóki nie zawołasz `/start_learning` albo nie podasz celu w RViz2.

### 5.2. Tematy dodane w tym ćwiczeniu

```bash
ros2 topic echo /collision      # std_msgs/msg/Bool: true po kolizji robota, false po resecie
ros2 topic echo /path           # trasa z A*
ros2 topic echo /cmd_vel --once # ostatnie polecenie prędkości
```

W kodzie: `collided` (`std::optional<bool>`) przechowuje ostatnią wiadomość z `/collision`, `publisher` publikuje na `/cmd_vel`.

### 5.3. Klienci serwisów i pliki

Szkielet pokazuje, jak węzeł sam woła serwisy (`reset_client->async_send_request(...)`). Plik Q-tablicy zapisujesz zwykłym `std::ofstream` — nie jest to nic ROS-owego.

## 6. Wskazówki implementacyjne

### 6.1. Przepływ jednego epizodu

W trybie uczenia szkielet wykonuje (poza `learning_loop`) wszystko sam:

1. reset robota w losowe miejsce → pauza `reset_settle_time`,
2. losowanie celu i publikacja na `/goal_pose` → A\* publikuje `/path`,
3. pauza `path_settle_time` → od tego momentu timer wywołuje `learning_loop()`,
4. `learning_loop()` kończy epizod przez `finish_episode("powód")`; ta funkcja czyści trasę i zleca kolejny reset (w trybie uczenia dodatkowo zapisuje Q-tablicę).

Epizod powinien się kończyć, gdy:

1. robot ma kolizję (`*collided == true`),
2. robot odjechał zbyt daleko od trasy (`find_head_point()` nie znajduje punktu — okrąg o promieniu `head_radius` nie przecina trasy),
3. robot dojechał do celu z pewną tolerancją (np. odległość do ostatniego punktu trasy < 0,2 m).

### 6.2. Poprzedni stan

Efekt akcji poznajesz dopiero **po** jej wykonaniu. Dlatego w kroku $t$ trzeba:

- obserwację zamienić na stan $s_t$,
- jeśli istnieje poprzedni krok (`previous_state`), policzyć nagrodę za *poprzednią akcję* i zaktualizować `Q(previous_state, previous_action)` przy użyciu $s_t$ jako następnego stanu,
- wybrać i wykonać nową akcję, po czym zapamiętać `s_t` i `a_t` jako poprzednie.

Pseudokod kroku:

```text
learning_loop():
    jeśli kolizja:
        jeśli uczymy się i jest poprzedni_stan:
            update(poprzedni_stan, poprzednia_akcja, kara, ..., terminalny = true)
        finish_episode("kolizja"); return
    punkt = find_head_point()
    jeśli brak punktu:
        (jak wyżej, z karą)   finish_episode("zjazd z trasy"); return
    obserwacja = [kąt do punktu, odległość do celu]
    s = discretize_state(obserwacja)
    cel_osiągnięty = odległość do celu < tolerancja
    jeśli uczymy się i jest poprzedni_stan:
        r = nagroda(...)
        update(poprzedni_stan, poprzednia_akcja, r, s, cel_osiągnięty)
    jeśli cel_osiągnięty: finish_episode("cel"); return
    a = choose_action(s)
    execute_action(a)
    poprzedni_stan = s; poprzednia_akcja = a
```

### 6.3. Nagrody

Nagroda jest jedyną informacją o tym, co jest „dobre". Pomysły do przemyślenia:

- duża kara za kolizję i zjechanie z trasy, nagroda za dotarcie do celu,
- mała kara za każdy krok (zachęta do szybkiego dojazdu),
- nagroda za zmniejszenie odległości do celu lub za mały kąt do punktu na trasie.

Do liczenia nagrody „za postęp" potrzebujesz wartości z poprzedniego kroku — dodaj odpowiednie pola do klasy. Uważaj na skalę: nagrody za postęp nie powinny przewyższać kary za kolizję.

### 6.4. Zapis i wczytanie

Wystarczy plik tekstowy: jeden wiersz = klucz stanu (liczby całkowite) + wartości Q dla wszystkich akcji. `load_q_table` jest wywoływane w konstruktorze; brak pliku to nie błąd. Zapis wywołują `finish_episode` oraz `/start_following`.

### 6.5. Zalecana kolejność pracy

1. Wypisz w logu obserwacje i zaimplementuj `discretize_state`; sprawdź ręcznie (`teleop` w RViz2), czy stany zmieniają się sensownie.
2. Zaimplementuj `execute_action` i wywołuj go z `learning_loop` na sztywno — sprawdź, czy robot wykonuje akcje.
3. Zaimplementuj `choose_action` (zachłanny, a potem ε-zachłanny) i `update_q_table`.
4. Napisz kompletną `learning_loop` (kolizja, zjazd z trasy, cel, nagrody).
5. Dodaj zapis i wczytanie; sprawdź, czy po restarcie węzła Q-tablica jest taka sama.
6. Uruchom `/start_learning`; loguj m.in. numer epizodu, wartość ε, wynik epizodu i rozmiar tablicy (`q_table.size()`).
7. Strój $\alpha$, $\gamma$, $\varepsilon$ i nagrody; dopiero potem przełącz na `/start_following` i oceń politykę.

Typowe błędy: aktualizacja Q przed poznaniem następnego stanu, brak obsługi stanu terminalnego, brak resetu `previous_state` po zakończeniu epizodu, ε stałe i zbyt duże/małe, zbyt wiele stanów.

## 7. Testowanie

| Scenariusz | Oczekiwany rezultat |
|------------|---------------------|
| uczenie w `cave` (kilkaset epizodów) | rosnąca liczba udanych epizodów, malejąca liczba kolizji |
| `/start_following` + cel z RViz2, prosta trasa | robot dojeżdża do celu |
| `/start_following`, trasa z zakrętami | robot dojeżdża bez kolizji i bez zjechania z trasy |
| restart węzła po nauce | polityka działa bez ponownego uczenia (wczytana Q-tablica) |
| kilka celów po sobie | każdy cel realizowany |
| uruchomienie w `hallway` i `lines` po sprawdzeniu w `cave` | sprawdzenie, jak polityka działa w innych światach |

## 8. Przydatne odnośniki

- Dokumentacja ROS 2 — serwisy: <https://docs.ros.org/en/jazzy/Tutorials/Beginner-CLI-Tools/Understanding-ROS2-Services/Understanding-ROS2-Services.html>
- Q-learning (Wikipedia): <https://en.wikipedia.org/wiki/Q-learning>
- Wykład si8 (materiały z zajęć)
