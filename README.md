# TRNG Generator - True Random Number Generator

Zaawansowany generator prawdziwie losowych liczb z funkcją podpisywania cyfrowego, wykorzystujący jitter entropii systemowej oraz instrukcje sprzętowe x86. Implementacja bazuje na koncepcjach z publikacji naukowej LETRNG (Lightweight and Efficient True Random Number Generator for GNU/Linux Systems).

## Funkcje

- **Prawdziwa losowość**: Wykorzystuje jitter czasowy, RDRAND/RDTSC oraz różne źródła entropii systemowej
- **Podpisywanie cyfrowe**: RSA-2048 z deterministycznym generowaniem kluczy
- **Analiza statystyczna**: Automatyczne obliczanie entropii i histogramów
- **Interfejs webowy**: Przyjazny interfejs Flask do podpisywania i weryfikacji plików
- **Wielowątkowość**: Optymalizacja wydajności przez równoległe zbieranie entropii
- **Weryfikacja**: Pełna weryfikacja podpisów cyfrowych

## Podstawy naukowe

Ten projekt implementuje koncepcje przedstawione w publikacji naukowej:

**Chen, Y., Zhu, F., Tian, Y., Xu, S., Lihong, H., Zhou, Q., & Ling, N. (2022). LETRNG — A Lightweight and Efficient True Random Number Generator for GNU/Linux Systems. *Tsinghua Science and Technology*, 28, 370-385. DOI: 10.26599/TST.2022.9010005**

### Kluczowe koncepcje z LETRNG

- **Jitter-based entropy collection**: Wykorzystanie niestabilności czasowej systemu jako źródła entropii
- **Hardware-assisted randomness**: Integracja z instrukcjami RDRAND/RDTSC dostępnymi w procesorach x86
- **Multi-threaded architecture**: Równoległe zbieranie entropii dla zwiększenia wydajności
- **Cryptographic post-processing**: Kondensacja entropii przez funkcje skrótu kryptograficzne
- **Real-time entropy analysis**: Monitorowanie jakości generowanych danych

### Rozszerzenia w tej implementacji

- **Podpisywanie cyfrowe RSA-2048**: Deterministyczne generowanie kluczy z entropii TRNG
- **Interfejs webowy**: Przyjazny dla użytkownika system podpisywania plików
- **Rozszerzona analiza**: Automatyczne generowanie histogramów i statystyk entropii
- **Optymalizacje wydajności**: Ulepszona architektura wielowątkowa i buforowanie

## Struktura projektu

```
trng-generator/
├── app.py                 # Główna aplikacja Flask
├── generator.c            # Generator TRNG w C
├── rsa_utils.py          # Utilities RSA i podpisywanie
├── run_trng.py           # Wrapper do uruchamiania generatora
├── templates/
│   └── index.html        # Template HTML
├── static/
│   └── style.css         # Stylowanie CSS
└── README.md
```

## Instalacja

### Wymagania systemowe

- **System**: Linux x86_64
- **Kompilator**: GCC z obsługą instrukcji x86
- **Python**: 3.7+
- **Biblioteki C**: OpenSSL, pthread

### Krok 1: Instalacja zależności systemowych

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install build-essential libssl-dev python3-pip
```

**CentOS/RHEL:**
```bash
sudo yum install gcc openssl-devel python3-pip
```

### Krok 2: Instalacja zależności Python

```bash
pip3 install flask pycryptodome
```

### Krok 3: Kompilacja generatora

```bash
gcc -o generator generator.c -lssl -lcrypto -lpthread -lm -O3 -march=native
```

### Krok 4: Uruchomienie aplikacji

```bash
python3 app.py
```

Aplikacja będzie dostępna pod adresem: `http://localhost:5000`

## Jak używać

### 1. Podpisywanie plików

1. Otwórz aplikację w przeglądarce
2. W sekcji "Podpisz plik" wybierz plik do podpisania
3. Kliknij "Podpisz plik"
4. System automatycznie:
   - Wygeneruje nowy zestaw danych losowych (13MB)
   - Utworzy deterministyczny klucz RSA-2048
   - Podpisze plik
   - Wyświetli podpis (hex) i klucz publiczny (PEM)

### 2. Weryfikacja podpisów

1. W sekcji "Weryfikacja podpisu":
   - Wybierz plik do weryfikacji
   - Wklej podpis w formacie hex
   - Wklej klucz publiczny w formacie PEM
2. Kliknij "Zweryfikuj podpis"
3. System wyświetli wynik: **POPRAWNY** lub **NIEPOPRAWNY**

## Szczegóły techniczne

### Generator TRNG

- **Źródła entropii**:
  - Jitter czasowy wysokiej rozdzielczości (`clock_gettime`)
  - Instrukcje RDRAND/RDTSC
  - Synchronizacja wątków
  - CPUID timing

- **Post-processing**: SHA3-512 dla kondensacji entropii
- **Wielowątkowość**: 4 wątki zbierające + 1 wątek zapisujący
- **Wydajność**: około 13MB danych w kilka sekund

### Kryptografia

- **Algorytm**: RSA-2048 z PKCS#1 v1.5
- **Hash**: SHA-256 dla podpisów
- **Deterministyczny RNG**: SHA-512 w trybie counter
- **Format kluczy**: PEM (Public Key Infrastructure)

### Analiza statystyczna

System automatycznie generuje:
- **Entropia**: Obliczenia Shannon entropy w bits/byte
- **Histogramy**: Rozkład częstotliwości bajtów
- **Pliki analityczne**:
  - `post_entropy.txt` - entropia danych po post-processingu
  - `post_histogram.txt` - histogram danych finalnych
  - `source_entropy.txt` - entropia danych surowych
  - `source_histogram.txt` - histogram danych surowych

## Pliki wyjściowe

Po uruchomieniu generatora tworzone są następujące pliki:

| Plik | Rozmiar | Opis |
|------|---------|------|
| `post.bin` | 13MB | Finalne dane losowe po SHA3-512 |
| `source.bin` | około 52MB | Surowe dane entropii |
| `*_entropy.txt` | <1KB | Analiza entropii |
| `*_histogram.txt` | <10KB | Histogramy rozkładu |

## Wydajność

- **Przepustowość**: około 3-4 MB/s danych losowych
- **Entropia**: Typowo >7.99 bits/byte
- **Czas generacji**: około 3-5 sekund dla 13MB
- **Zużycie CPU**: Wykorzystuje wielowątkowość

## Bezpieczeństwo

### Zalety
- Prawdziwa entropia systemowa
- Kryptograficznie silne post-processing
- Deterministyczne klucze (reprodukowalność)
- Standardowe algorytmy kryptograficzne

### Ograniczenia
- Wymaga sprzętu x86 z RDRAND
- Jakość entropii zależy od systemu
- Deterministyczne klucze (przewidywalność przy tym samym źródle)

## Testowanie jakości

Zalecane narzędzia do testowania:

```bash
# Test NIST
./sts-2.1.2/assess post.bin

# Test Dieharder
dieharder -a -g 201 -f post.bin

# Test ENT
ent post.bin
```

Oczekiwane wyniki:
- **Entropia**: >7.99 bits/byte
- **Chi-square**: p-value >0.01
- **Kompresja**: <1% dla dobrych danych

## Licencja

Projekt dostępny na licencji MIT. Zobacz plik `LICENSE` dla szczegółów.

## Bibliografia i źródła

1. **LETRNG Research Paper**:
   - Chen, Y., Zhu, F., Tian, Y., Xu, S., Lihong, H., Zhou, Q., & Ling, N. (2022). LETRNG — A Lightweight and Efficient True Random Number Generator for GNU/Linux Systems. *Tsinghua Science and Technology*, 28, 370-385. DOI: 10.26599/TST.2022.9010005

2. **Standardy kryptograficzne**:
   - NIST SP 800-90A: Recommendation for Random Number Generation Using Deterministic Random Bit Generators
   - FIPS 140-2: Security Requirements for Cryptographic Modules
   - RFC 3447: Public-Key Cryptography Standards (PKCS) #1: RSA Cryptography Specifications

3. **Dokumentacja techniczna**:
   - Intel Digital Random Number Generator (DRNG) Software Implementation Guide
   - OpenSSL Cryptography and SSL/TLS Toolkit Documentation

## Kontakt

- **Autor**: Igor Suchodolski
- **Email**: igor.suchodolskii@gmail.com
