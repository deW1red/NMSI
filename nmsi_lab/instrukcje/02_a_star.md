# Planowanie trasy algorytmem A\*

Przed rozpoczęciem tej części wykonaj `01_wprowadzenie_ros2.md` — zakładamy, że umiesz uruchomić symulację, zbudować pakiet i obserwować tematy w terminalu.

## 1. Cel ćwiczenia

Węzeł `a_star` odbiera cel z RViz2 (`/goal_pose`), wyznacza pozycję startową robota z TF i wywołuje metodę:

```cpp
void AStarCalculator::a_star(PoseStamped start, PoseStamped end);
```

Pakiet zawiera dwa równoważne szkielety:

- C++: `nmsi_lab/src/a_star.cpp` — uzupełnij metodę `AStarCalculator::a_star`,
- Python: `nmsi_lab/src/a_star.py` — uzupełnij metodę `AStarCalculator.a_star`.

W wybranym wariancie zaimplementuj algorytm A\*, aby wyznaczał optymalną trasę na mapie i publikował ją na temat `/path` (typ `nav_msgs/msg/Path`). Pozostałe elementy interfejsu są przygotowane w szablonie. Resztą (jazda po trasie) zajmuje się gotowy węzeł `path_follower`.

Nie zmieniaj nazw tematów ani zachowania pozostałych węzłów.

Po zbudowaniu pakietu uruchom symulację i dokładnie jeden wariant kontrolera:

```bash
# terminal 1
ros2 launch nmsi_lab simulation.launch.py world:=hallway
# terminal 2 — wybierz C++:
ros2 launch nmsi_lab robot_controller.launch.py
# albo zamiast powyższego wybierz Python:
ros2 launch nmsi_lab robot_controller_python.launch.py
```

Oba pliki kontrolera uruchamiają `path_follower` oraz węzeł `a_star` o tej samej nazwie, dlatego nie uruchamiaj ich jednocześnie.

## 2. Punktacja i zasady

### 2.1. Punktacja

| Zadanie | Opis | Punkty |
|---------|------|--------|
| 1 | Podstawowy A\*: obsługa mapy rozszerzonej (`/map_dilated`), sprawdzanie dostępności komórek i sąsiadów; sąsiedztwo czterospójne | 5 |
| 2 | Uwzględnienie 8 sąsiadów | 1,5 |
| 3 | Uwzględnienie mapy kosztów `/map_cost` | 1,5 |
| 4 | Zmodyfikowany A\* (ze zbiorem przejrzanych) | 2 |
| Bonus | Samodzielna modyfikacja mapy kosztów lub publikacja własnej mapy, która uwzględnia dodatkowe właściwości (np. koszt nieliniowy względem odległości od przeszkód). Zachęcamy do kreatywnych rozwiązań. | +1 |

Zwykły termin oddania: **8 listopada**. Termin zapasowy: **15 listopada** — za pracę oddaną w tym terminie przyznawane jest 75% punktów zdobytych za zadania w zwykłym terminie. Po tym terminie zero.

Kod źródłowy należy przesłać przez platformę **LeON**. Wysyłaj wyłącznie pliki źródłowe, bez katalogów `build`, `install` i `log`. Przy wyborze Pythona dołącz także zmienione pliki konfiguracji pakietu i launch, jeśli były potrzebne do uruchomienia rozwiązania.

### 2.2. Samodzielność pracy

Rozwiązanie można przygotować w **C++ lub Pythonie**.

> **Zadanie trzeba wykonać samodzielnie.**
>
> Wolno korzystać z: własnej wiedzy, materiałów przygotowanych przez prowadzącego (w tym tej instrukcji i pseudokodu), dokumentacji ROS 2 oraz wybranego języka i jego biblioteki standardowej (np. `std::map`, `std::unordered_map`, `std::vector` w C++ albo `dict`, `list`, `set` w Pythonie).
>
> Nie wolno korzystać z: bibliotek do obsługi grafów i planowania trasy (np. Nav2 planner, LEMON, Boost.Graph), gotowych fragmentów kodu z internetu, kodu innych osób ani **kodu wygenerowanego przez modele językowe (LLM) i inne narzędzia AI**.
>
> Wykrycie pracy wykonanej niesamodzielnie lub z użyciem kodu wygenerowanego przez LLM skutkuje:
>
> - **wyzerowaniem punktów za dane zadanie,**
> - **utratą wszystkich punktów bonusowych** (zarówno z wcześniejszych, jak i z późniejszych zadań),
> - **odjęciem jednego punktu od każdego z pozostałych zadań.**
>
> Prowadzący może poprosić o omówienie napisanego kodu.

W razie wątpliwości, czy coś jest dozwolone, **zapytaj prowadzącego zanim to wykorzystasz**.

## 3. Algorytm A\* — teoria

### 3.1. Grafy

**Graf** to zbiór wierzchołków $V$ oraz zbiór krawędzi $E$ łączących pary wierzchołków. Krawędzie mogą mieć **wagi** (np. odległość lub koszt przejścia). W grafie nieskierowanym krawędź łączy wierzchołki w obie strony. Graf można zapisać m.in. jako macierz sąsiedztwa, listy sąsiedztwa lub macierz incydencji.

W tym ćwiczeniu graf **nie jest dany w całości z góry** — budujemy go na bieżąco na podstawie mapy:

| Element grafu | W naszym zadaniu |
|---------------|------------------|
| wierzchołek | wolna komórka mapy $(x, y)$ |
| krawędź | połączenie komórek sąsiednich (4 lub 8 kierunków) |
| waga krawędzi | odległość między środkami komórek: $r$ dla ruchu w poziomie/pionie, $r\sqrt{2}$ po skosie ($r$ — rozdzielczość mapy w metrach) |

Sąsiadów komórki wyznaczasz „w locie" w momencie, gdy jej potrzebujesz. Z tego powodu nie ma sensu z góry wypełniać tablicy $g$ nieskończonościami dla wszystkich komórek — mapa `cave` ma ok. 250 tys. komórek. W pseudokodzie poniżej wartości $g$ zapisujemy tylko dla komórek, które faktycznie odwiedziliśmy.

### 3.2. Idea algorytmu

A\* szuka najkrótszej ścieżki z wierzchołka startowego do celu. Dla każdego wierzchołka $x$ utrzymuje:

- $g(x)$ — koszt najlepszej dotychczas znalezionej drogi ze startu do $x$,
- $h(x)$ — **heurystyka**, czyli oszacowanie kosztu drogi z $x$ do celu,
- $f(x) = g(x) + h(x)$ — szacowany koszt całej drogi przechodzącej przez $x$.

W każdym kroku algorytm rozwija wierzchołek o **najmniejszej wartości $f$** spośród dotychczas napotkanych, ale jeszcze nierozwiniętych. Rozwinięcie polega na przejrzeniu sąsiadów i — jeśli przez ten wierzchołek da się do któregoś z nich dojść taniej niż dotychczas — zaktualizowaniu $g$ sąsiada oraz zapamiętaniu, skąd do niego przyszliśmy. Gdy rozwijanym wierzchołkiem jest cel, trasę odtwarzamy cofając się po zapamiętanych poprzednikach.

Dla $h \equiv 0$ A\* sprowadza się do algorytmu Dijkstry. Dobra heurystyka pozwala przeszukać znacznie mniej wierzchołków.

### 3.3. Heurystyka

W tym ćwiczeniu użyj **odległości euklidesowej** od komórki do celu (w metrach, zgodnie z jednostką wag krawędzi):

$$h(x) = \sqrt{(x_1 - x_g)^2 + (y_1 - y_g)^2}$$

gdzie $(x_1, y_1)$ to środek komórki $x$, a $(x_g, y_g)$ to środek komórki docelowej.

Własności heurystyki:

- **dopuszczalna (admissible)** — nigdy nie zawyża rzeczywistego kosztu dojścia do celu: $h(x) \le h^*(x)$. Gwarantuje, że podstawowy A\* znajdzie trasę optymalną.
- **spójna (consistent)** — dla każdej krawędzi $(x, y)$ zachodzi $h(x) \le h(y) + d(x, y)$, gdzie $d$ to waga krawędzi. Gwarantuje, że zmodyfikowany A\* (ze zbiorem przejrzanych) znajdzie trasę optymalną, a każdy wierzchołek wystarczy rozwinąć tylko raz.

Odległość euklidesowa spełnia oba warunki, o ile wagi krawędzi nie są mniejsze niż odległość euklidesowa między końcami krawędzi. Pamiętaj o tym, jeśli zdecydujesz się modyfikować wagi (zadanie bonusowe).

### 3.4. Zbiór otwarty i wybór kolejnego wierzchołka

**Zbiór otwarty** zawiera odkryte wierzchołki, które nie zostały jeszcze rozwinięte. Dla każdego wierzchołka przechowuj w nim tylko jeden bieżący priorytet $f$. Gdy znajdziesz lepszą drogę do wierzchołka, zaktualizuj jego koszt i priorytet w zbiorze otwartym — nie dodawaj drugiego wpisu, który mógłby mieć nieaktualną wartość. W każdej iteracji wybieraj i usuwaj wierzchołek o najmniejszym $f$.

Sposób przechowywania zbioru zależy od języka i rozwiązania. Możesz użyć np. mapy/słownika indeksowanego wierzchołkiem i wyszukiwać minimum, albo struktury uporządkowanej według pary $(f, \text{identyfikator wierzchołka})$, która pozwala sprawnie aktualizować wpis i pobierać minimum. Wybór konkretnego kontenera nie jest narzucony; ważne, by aktualizacja nie pozostawiała nieaktualnych duplikatów.

### 3.5. Dwie wersje algorytmu

**Wersja podstawowa** nie pamięta, które wierzchołki zostały już rozwinięte. Jeśli po rozwinięciu wierzchołka zostanie znaleziona do niego lepsza droga, może on wrócić do zbioru otwartego i zostać rozwinięty ponownie. Trasa jest optymalna, jeśli heurystyka jest dopuszczalna.

**Wersja zmodyfikowana** utrzymuje dodatkowo **zbiór przejrzanych** wierzchołków (rozwiniętych) i pomija je przy rozpatrywaniu sąsiadów. Jest szybsza, ale gwarantuje optymalność tylko dla heurystyki **spójnej**. Przy spójnej heurystyce, takiej jak używana w tym ćwiczeniu, nie ma potrzeby ponownego otwierania rozwiniętych wierzchołków.

## 4. Pseudokod

Oznaczenia: `otwarte` — mapa/słownik przechowujący dla każdego wierzchołka w zbiorze otwartym jego bieżące $f$; `g` i `przyszedłZ` — mapy/słowniki, w których **brak klucza oznacza wartość nieskończoną / brak poprzednika** (nie inicjalizujemy ich dla całego grafu); `waga(x, y)` — waga krawędzi; `sąsiedzi(x)` — dostępni sąsiedzi komórki `x`.

### 4.1. Wersja podstawowa

```
funkcja A*(start, meta):
    g := pusta mapa
    przyszedłZ := pusta mapa
    g[start] := 0
    otwarte := pusta mapa
    otwarte[start] := g[start] + h(start)

    dopóki otwarte nie jest puste:
        x := wybierz i usuń z otwarte wierzchołek o najmniejszym f
        jeśli x = meta:
            zwróć zrekonstruuj_trasę(przyszedłZ, meta)
        dla każdego y spośród sąsiedzi(x):
            tymczasowe_g := g[x] + waga(x, y)
            jeśli y nie ma w g lub tymczasowe_g < g[y]:
                przyszedłZ[y] := x
                g[y] := tymczasowe_g
                otwarte[y] := g[y] + h(y)
    zwróć porażka
```

Uwagi:

- Warunek „`y` nie ma w `g`" zastępuje porównanie z nieskończonością.
- Przypisanie `otwarte[y]` dodaje wierzchołek albo aktualizuje jego bieżący priorytet. W zbiorze otwartym nie ma kilku wpisów dla tego samego wierzchołka.
- Po rozwinięciu wierzchołek nie znajduje się już w `otwarte`; jeśli później zostanie znaleziona do niego lepsza droga, może zostać dodany ponownie.

### 4.2. Wersja zmodyfikowana (ze zbiorem przejrzanych)

```
funkcja A*(start, meta):
    przejrzane := pusty zbiór
    g := pusta mapa
    przyszedłZ := pusta mapa
    g[start] := 0
    otwarte := pusta mapa
    otwarte[start] := g[start] + h(start)

    dopóki otwarte nie jest puste:
        x := wybierz i usuń z otwarte wierzchołek o najmniejszym f
        jeśli x = meta:
            zwróć zrekonstruuj_trasę(przyszedłZ, meta)
        dodaj x do przejrzane
        dla każdego y spośród sąsiedzi(x):
            jeśli y w przejrzane:
                continue
            tymczasowe_g := g[x] + waga(x, y)
            jeśli y nie ma w g lub tymczasowe_g < g[y]:
                przyszedłZ[y] := x
                g[y] := tymczasowe_g
                otwarte[y] := g[y] + h(y)
    zwróć porażka
```

Ponieważ w tej wersji sąsiedzi należący do `przejrzane` są pomijani, rozwinięty wierzchołek nie wraca do zbioru otwartego. Ta wersja wymaga spójnej heurystyki, zgodnie z punktem 3.5.

### 4.3. Odtwarzanie trasy

```
funkcja zrekonstruuj_trasę(przyszedłZ, obecny):
    trasa := [obecny]
    dopóki obecny ma wpis w przyszedłZ:
        obecny := przyszedłZ[obecny]
        trasa.dodaj_na_początek(obecny)
    zwróć trasa
```

(Alternatywnie dodawaj na koniec i odwróć listę na końcu.)

### 4.4. Samodzielne prześledzenie algorytmu

Przed kodowaniem prześledź algorytm na niewielkim grafie z wagami krawędzi i wartościami heurystyki. Zapisz kolejno wartości `g` i `f`, poprzedników oraz wybierane wierzchołki. Pamiętaj, że pseudokod przechowuje wpisy `g` tylko dla odkrytych wierzchołków; brak klucza oznacza, że koszt nie został jeszcze wyznaczony.

## 5. Mapa w ROS 2 — jak dostać się do komórek

### 5.1. Struktura `nav_msgs/msg/OccupancyGrid`

```
header                         # frame_id: "map"
info:
  resolution                   # rozmiar jednej komórki w metrach [m/komórkę]
  width                        # liczba komórek w poziomie (oś x)
  height                       # liczba komórek w pionie (oś y)
  origin                       # pozycja (geometry_msgs/Pose) lewego dolnego rogu komórki (0, 0) w układzie mapy
data                           # tablica int8 o długości width * height
```

Komórki są ułożone **wierszami**: komórka o indeksach $(ix, iy)$ (kolumna, wiersz) leży pod indeksem

$$\text{idx} = iy \cdot \text{width} + ix$$

Wartości w `data`:

| Wartość | Znaczenie |
|---------|-----------|
| `-1` | obszar nieznany |
| `0` | wolne |
| `100` | zajęte |
| `1…99` | prawdopodobieństwo zajętości (w `/map_cost` — koszt) |

### 5.2. Wskazówki dotyczące odczytu

W C++ pola wiadomości odczytujesz tak jak pola struktury, np. `map_dilated->info.width`, `map_dilated->info.origin.position.x`, `map_dilated->data[idx]`. W Pythonie analogiczne pola są dostępne przez atrybuty, np. `map_dilated.info.width` i `map_dilated.data[idx]`. Mapa **może jeszcze nie być dostępna** (w C++ wskaźnik jest pusty, w Pythonie wartość może być `None`); sprawdź to przed użyciem i zgłoś ostrzeżenie w logu.

### 5.3. Konwersje współrzędnych

Przeliczenie punktu $(x, y)$ w metrach (układ `map`) na indeksy komórki oraz z powrotem (środek komórki):

$$ix = \left\lfloor \frac{x - o_x}{r} \right\rfloor, \qquad iy = \left\lfloor \frac{y - o_y}{r} \right\rfloor$$

$$x = o_x + (ix + 0{,}5)\, r, \qquad y = o_y + (iy + 0{,}5)\, r$$

gdzie $(o_x, o_y)$ to `info.origin.position`, a $r$ to `info.resolution`. Mapy w tym ćwiczeniu nie są obrócone (orientacja `origin` jest jednostkowa), więc obrót możesz pominąć.

Przy dzieleniu pamiętaj o rzutowaniu na `double`. Przed użyciem indeksów sprawdź, czy mieszczą się w zakresie $[0, \text{width})$ i $[0, \text{height})$ — punkt wskazany przez użytkownika może leżeć poza mapą.

### 5.4. Którą mapę wykorzystać

| Mapa | Zastosowanie |
|------|--------------|
| `/map_dilated` | **sprawdzanie, czy komórka jest dostępna**. Przeszkody są pogrubiane o promień robota, więc robota można traktować jak punkt. W konfiguracji `simulation.launch.py` parametr `robot_radius` wynosi obecnie 0,5 m; jeśli parametr zostanie zmieniony, obowiązuje wartość z uruchomienia. Komórkę uznaj za wolną, gdy `0 <= data < 65`; wartość `-1` (nieznane) i wartości `>= 65` traktuj jako przeszkodę. |
| `/map_cost` | koszt 0–100 rosnący w miarę zbliżania się do przeszkód (0 = daleko od przeszkód); wykorzystywany w zadaniu 3. Wartość `-1` oznacza obszar nieznany, a nie koszt do użycia we wzorze. |

Obie mapy mają ten sam rozmiar, rozdzielczość i origin jak `/map`.

### 5.5. Konstrukcja wyniku — `nav_msgs/msg/Path`

`Path` to nagłówek i wektor `poses` typu `PoseStamped`. Wymagania `path_follower`:

- trasa musi mieć co najmniej 2 punkty; **pierwszy punkt jest pomijany** (to pozycja startowa), robot jedzie do kolejnych,
- każdy punkt powinien mieć `header.frame_id = "map"`,
- każdy punkt musi mieć poprawną orientację — ustaw `pose.orientation.w = 1.0`. Domyślnie wszystkie składowe kwaternionu są zerowe, co jest nieprawidłowe i spowoduje błędy transformacji TF w `path_follower`,
- wypełniaj `pose.position.x/y` współrzędnymi środków komórek trasy w metrach.

Sugerowana kolejność punktów trasy: `start` (dokładna pozycja robota) → środki kolejnych komórek trasy → `end` (dokładny cel z `/goal_pose`).

Gdy trasa nie istnieje, wypisz w logu komunikat **„Brak"** (np. `RCLCPP_WARN` w C++ lub `self.get_logger().warning(...)` w Pythonie) i **nie publikuj** żadnej trasy.

## 6. Zadania

Przygotuj funkcje pomocnicze (jako metody klasy lub funkcje poza klasą), które ułatwią rozwiązanie. Poniższe odpowiedzialności są wskazówkami, a nie wymaganym podziałem na funkcje ani narzuconymi sygnaturami — możesz je łączyć lub dzielić zależnie od rozwiązania:

1. konwersja współrzędnych w metrach na komórkę oraz wyznaczenie współrzędnych jej środka (punkt 5.3),
2. sprawdzenie, czy komórka mieści się w mapie i jest dostępna na podstawie `/map_dilated`, oraz bezpieczny odczyt jej wartości,
3. wyznaczenie sąsiadów i wag krawędzi dla aktualnego etapu ćwiczenia,
4. obliczenie heurystyki (punkt 3.3) oraz odtworzenie trasy z mapy poprzedników (punkt 4.3).

Wskazówki i pułapki:

- Pozycja startowa robota może leżeć w obszarze oznaczonym jako niedostępny na `/map_dilated` (np. gdy robot stoi blisko ściany) — jeśli leży w mapie, nie odrzucaj startu z tego powodu. Start poza mapą albo cel w przeszkodzie lub poza mapą oznacza „Brak".
- Przy identyfikowaniu komórki pamiętaj o indeksie wierszowym z punktu 5.1 i sprawdzaj granice przed odczytem mapy. Wygodnym identyfikatorem wierzchołka może być para indeksów komórki albo pojedynczy `idx`.

### Zadanie 1 — A\* podstawowy (4-sąsiedztwo)

Zaimplementuj A\* według pseudokodu 4.1, korzystając z `/map_dilated` do budowania grafu w locie. Na tym etapie uwzględnij czterech sąsiadów (góra, dół, lewo, prawo); każda krawędź ma wagę równą rozdzielczości mapy $r$. Wynikiem ma być trasa opublikowana na `/path` albo komunikat „Brak".

### Zadanie 2 — 8-sąsiedztwo

Rozszerz graf o ruch po przekątnej. Waga krawędzi wynosi $r$ dla ruchu poziomego/pionowego oraz $r\sqrt{2}$ dla ruchu po skosie.

### Zadanie 3 — mapa kosztów

Uwzględnij mapę `/map_cost`, aby trasa trzymała się z dala od ścian (a nie tylko omijała pogrubione przeszkody). Zwiększ wagę krawędzi proporcjonalnie do kosztu komórki docelowej, np.:

$$w(x, y) = d(x, y) \cdot \left(1 + k \cdot \frac{\text{cost}(y)}{100}\right), \quad k \ge 0$$

Tutaj $d(x,y)$ to odległość geometryczna między komórkami, a $\text{cost}(y)$ to wartość komórki docelowej odczytana z `/map_cost`. Dla $k \ge 0$ waga nie jest mniejsza niż odległość geometryczna, więc heurystyka euklidesowa pozostaje dopuszczalna i spójna. Nie podstawiaj wartości `-1` za obszar nieznany do tego wzoru. Porównaj wynik dla różnych $k$.

### Zadanie 4 — A\* zmodyfikowany

Rozszerz implementację o zbiór przejrzanych wierzchołków zgodnie z pseudokodem 4.2. Porównaj obie wersje dla tych samych map, wag, heurystyki i punktów start–cel: wypisz liczbę rozwiniętych wierzchołków oraz łączny koszt trasy (możesz podać także jej długość geometryczną). Przy wielu trasach o takim samym koszcie ich przebieg może się różnić; porównuj przede wszystkim optymalny koszt.

### Zadanie bonusowe — modyfikacja mapy kosztów

Samodzielnie zmodyfikuj sposób wyznaczania kosztu lub opublikuj własną mapę uwzględniającą dodatkowe właściwości, np. koszt nieliniowy względem odległości od przeszkód.

## 7. Testowanie

Przetestuj rozwiązanie w co najmniej kilku scenariuszach (świat wybierasz parametrem `world:=` w `simulation.launch.py`):

| Scenariusz | Oczekiwany rezultat |
|------------|---------------------|
| cel w tym samym pomieszczeniu, bez przeszkód | trasa zbliżona do linii prostej |
| `hallway`: cel za zielonym blokiem | trasa omija blok, nie przechodzi przez ścianę |
| `cave`, `lines` | trasa przez wąskie przejścia, robot nie zderza się ze ścianami |
| cel w ścianie lub poza mapą | komunikat „Brak", brak nowej trasy |
| cel w obszarze niedostępnym (odseparowanym) | komunikat „Brak" |
| start i cel blisko siebie | poprawna, krótka trasa (min. 2 punkty) |
| kilka celów po sobie | za każdym razem poprawna nowa trasa |

Wskazówki dotyczące debugowania:

- Wypisuj w logu (np. `RCLCPP_INFO` w C++ lub `self.get_logger().info(...)` w Pythonie) współrzędne i indeksy startu oraz celu, rozmiar mapy i liczbę rozwiniętych wierzchołków.
- Wyświetl w RViz2 `/map_dilated`, żeby zobaczyć, gdzie algorytm może przechodzić (punkt 4.9 w `01_wprowadzenie_ros2.md`).
- Najpierw sprawdź konwersje współrzędnych na prostych przykładach — typowe błędy to zamienione osie `x`/`y`, pominięty `origin` i indeks `iy * width + ix` zapisany jako `ix * height + iy`.
- Dla małych map (`hallway`, rozdzielczość 0,1 m) A\* działa natychmiast. Mapy `cave` i `lines` mają rozdzielczość 0,032 m (ok. 250 tys. komórek). Wielokrotne liniowe przeszukiwanie całego zbioru otwartego w celu znalezienia minimum może być na nich wyraźnie wolniejsze; w razie potrzeby wybierz strukturę danych, która sprawnie aktualizuje priorytety i pobiera minimum bez utrzymywania nieaktualnych duplikatów.
- Dla `h ≡ 0` otrzymasz algorytm Dijkstry — możesz porównać liczbę rozwiniętych wierzchołków z wersją z heurystyką euklidesową.
- Jeżeli robot jedzie nieregularnie po trasie z gęsto rozmieszczonymi punktami (w `cave` odległość między komórkami to 3 cm, a `path_follower` uznaje punkt za osiągnięty w promieniu 5 cm), możesz opublikować co kilka punktów trasy, pamiętając o zachowaniu ostatniego.

## 8. Przydatne odnośniki

- Dokumentacja ROS 2: <https://docs.ros.org/en/lyrical/ROS-Framework.html>
- A\* (Wikipedia, pełny pseudokod): <https://en.wikipedia.org/wiki/A*_search_algorithm>
