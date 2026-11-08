// FnFNitro's Mods page text in the game's language.
//
// The language is the game's text language, as FH2 ANNA and Extra Options read it: the string tables' manager
// ([game+0x3662990]) holds its code, an MSVC std::string ("EN", "DE", "CHT"...), at +0x30. The Mods page's rows
// are added before the game has a language, so they're in the one seen last time (FnFNitro.ini, [Language] Last).
// Czech and Hungarian stay English: the game's own text is English there.
//
// Every translation uses only characters the game's fonts for that language have (media\ui\Fonts.zip: Anthem
// for Latin text, with Verdana for Russian, and for Japanese, Korean and Chinese a font holding only the
// characters the game's own text uses), so none shows as a box. Their terms are the game's own (Rewind, Fast
// Travel, Drivatar, Vibration...). Chinese says NOS: its font has no 氮. Check new text against the fonts.
#include "lang.h"

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <cwchar>

namespace {

constexpr uintptr_t kStringsManager = 0x3662990;

// The languages, in the table's order. English is the key itself (EN, GB, CZ, HU, DEV, LOC).
const char* const kCodes[] = {"BR", "DE", "FR", "ES", "MX", "IT", "JP", "KO", "CHT", "PL", "RU", "NL"};
constexpr int kLangs = sizeof kCodes / sizeof kCodes[0];

struct Text {
    const char* en;
    const char* t[kLangs];
};

// clang-format off
const Text kTexts[] = {
{"Fast & Furious nitro for every car in Forza Horizon 2, with exhaust flames, sound and a NOS gauge. Hold the Nitro Button (LB unless you change it) to boost.",
 {"Nitro de Fast & Furious para todos os carros do Forza Horizon 2, com chamas no escape, som e medidor de NOS. Segure o Botão do Nitro (LB, a menos que você mude) para acelerar.",
  "Fast & Furious-Nitro für jedes Auto in Forza Horizon 2, mit Auspuffflammen, Sound und NOS-Anzeige. Halte die Nitro-Taste (LB, falls nicht geändert), um zu boosten.",
  "La nitro Fast & Furious pour toutes les voitures de Forza Horizon 2, avec flammes à l'échappement, son et jauge de NOS. Maintenez le bouton de nitro (LB par défaut) pour accélérer.",
  "Nitro de Fast & Furious para todos los coches de Forza Horizon 2, con llamas en el escape, sonido e indicador de NOS. Mantén el botón de nitro (LB si no lo cambias) para acelerar.",
  "Nitro de Fast & Furious para todos los autos de Forza Horizon 2, con llamas en el escape, sonido e indicador de NOS. Mantén el botón de nitro (LB si no lo cambias) para acelerar.",
  "Il nitro di Fast & Furious per tutte le auto di Forza Horizon 2, con fiamme dallo scarico, suono e indicatore NOS. Tieni premuto il pulsante del nitro (LB se non lo cambi) per accelerare.",
  "Forza Horizon 2の全車にFast & Furiousのニトロを追加します。排気の火、音、NOSゲージ付き。ニトロボタン（初期設定はLB）を押し続けるとブーストします。",
  "Forza Horizon 2의 모든 차에 Fast & Furious 니트로를 더합니다. 배기 화염, 소리, NOS 게이지 포함. 니트로 버튼(기본 LB)을 누르고 있으면 부스트합니다.",
  "為 Forza Horizon 2 的所有車輛加入 Fast & Furious NOS，附排氣噴火、音效與 NOS 計量表。按住 NOS 按鈕 (預設為 LB) 即可加速。",
  "Nitro z Fast & Furious dla każdego auta w Forza Horizon 2, z płomieniami z wydechu, dźwiękiem i wskaźnikiem NOS. Przytrzymaj przycisk nitro (domyślnie LB), aby przyspieszyć.",
  "Нитро в стиле Fast & Furious для всех машин Forza Horizon 2 — с пламенем из выхлопа, звуком и шкалой NOS. Удерживайте кнопку нитро (по умолчанию LB) для ускорения.",
  "Fast & Furious-nitro voor elke auto in Forza Horizon 2, met uitlaatvlammen, geluid en een NOS-meter. Houd de nitroknop (standaard LB) ingedrukt om te boosten."}},
{"Nitro System",
 {"Sistema de Nitro",
  "Nitro-System",
  "Système de nitro",
  "Sistema de nitro",
  "Sistema de nitro",
  "Sistema nitro",
  "ニトロシステム",
  "니트로 시스템",
  "NOS 系統",
  "System nitro",
  "Система нитро",
  "Nitrosysteem"}},
{"Turns the Fast & Furious nitro on or off. Hold the Nitro Button while driving to boost.",
 {"Liga ou desliga o nitro de Fast & Furious. Segure o Botão do Nitro enquanto dirige para acelerar.",
  "Schaltet das Fast & Furious-Nitro ein oder aus. Halte beim Fahren die Nitro-Taste gedrückt, um zu boosten.",
  "Active ou désactive la nitro Fast & Furious. Maintenez le bouton de nitro en conduisant pour accélérer.",
  "Activa o desactiva el nitro de Fast & Furious. Mantén pulsado el botón de nitro mientras conduces para acelerar.",
  "Activa o desactiva el nitro de Fast & Furious. Mantén presionado el botón de nitro mientras manejas para acelerar.",
  "Attiva o disattiva il nitro di Fast & Furious. Tieni premuto il pulsante del nitro durante la guida per accelerare.",
  "Fast & Furiousのニトロをオン/オフします。走行中にニトロボタンを押し続けるとブーストします。",
  "Fast & Furious 니트로를 켜거나 끕니다. 주행 중 니트로 버튼을 누르고 있으면 부스트합니다.",
  "開啟或關閉 Fast & Furious NOS。駕駛時按住 NOS 按鈕即可加速。",
  "Włącza lub wyłącza nitro z Fast & Furious. Przytrzymaj przycisk nitro podczas jazdy, aby przyspieszyć.",
  "Включает или выключает нитро в стиле Fast & Furious. Удерживайте кнопку нитро во время езды для ускорения.",
  "Zet de Fast & Furious-nitro aan of uit. Houd tijdens het rijden de nitroknop ingedrukt om te boosten."}},
{"Nitro Debug Overlay",
 {"Sobreposição de depuração do nitro",
  "Nitro-Debug-Anzeige",
  "Superposition de débogage nitro",
  "Superposición de depuración del nitro",
  "Superposición de depuración del nitro",
  "Overlay di debug del nitro",
  "ニトロのデバッグ表示",
  "니트로 디버그 오버레이",
  "NOS 除錯資訊",
  "Nakładka debugowania nitro",
  "Отладочный оверлей нитро",
  "Nitro-debugoverlay"}},
{"Draws nitro and flame debug info over the game (needs borderless/windowed mode).",
 {"Mostra informações de depuração do nitro e das chamas sobre o jogo (requer modo janela ou sem bordas).",
  "Zeigt Debug-Infos zu Nitro und Flammen über dem Spiel an (braucht den randlosen oder Fenstermodus).",
  "Affiche par-dessus le jeu les infos de débogage de la nitro et des flammes (mode fenêtré ou sans bordure requis).",
  "Muestra sobre el juego información de depuración del nitro y las llamas (requiere modo ventana o sin bordes).",
  "Muestra sobre el juego información de depuración del nitro y las llamas (requiere modo ventana o sin bordes).",
  "Mostra sopra il gioco le informazioni di debug del nitro e delle fiamme (richiede la modalità finestra o senza bordi).",
  "ニトロと火のデバッグ情報をゲーム画面に表示します（ボーダーレスまたはウィンドウモードが必要）。",
  "니트로와 화염 디버그 정보를 게임 위에 표시합니다(테두리 없는 창 또는 창 모드 필요).",
  "在遊戲畫面上顯示 NOS 與噴火的除錯資訊 (需要視窗或無邊界視窗模式)。",
  "Wyświetla na grze informacje debugowania nitro i płomieni (wymaga trybu okna lub okna bez ramki).",
  "Выводит поверх игры отладочную информацию о нитро и пламени (нужен оконный режим или режим без рамки).",
  "Toont debuginfo over nitro en vlammen boven het spel (vereist venstermodus of randloos)."}},
{"Backfire on All Cars",
 {"Estouros no escape em todos os carros",
  "Fehlzündungen bei allen Autos",
  "Retours de flamme sur toutes les voitures",
  "Petardeo en todos los coches",
  "Petardeo en todos los autos",
  "Ritorni di fiamma su tutte le auto",
  "全車アフターファイア",
  "모든 차에 백파이어",
  "所有車輛回火",
  "Strzały z wydechu we wszystkich autach",
  "Выстрелы выхлопа у всех машин",
  "Knallende uitlaat bij alle auto's"}},
{"Lets every car backfire, which the nitro exhaust flames need. Off leaves the game's own backfire settings alone. Takes effect after a restart.",
 {"Permite que todos os carros estourem no escape, o que as chamas do nitro precisam. Desligado mantém as configurações do próprio jogo. Vale após reiniciar o jogo.",
  "Lässt jedes Auto Fehlzündungen haben, die die Nitro-Auspuffflammen brauchen. Aus lässt die Einstellungen des Spiels unverändert. Wirkt nach einem Neustart.",
  "Permet à chaque voiture d'avoir des retours de flamme, nécessaires aux flammes de la nitro. Désactivé, les réglages du jeu restent inchangés. Prend effet après un redémarrage.",
  "Permite que todos los coches petardeen, algo que necesitan las llamas del nitro. Desactivado, se mantienen los ajustes del propio juego. Se aplica al reiniciar el juego.",
  "Permite que todos los autos petardeen, algo que necesitan las llamas del nitro. Desactivado, se mantiene la configuración del propio juego. Se aplica al reiniciar el juego.",
  "Consente a ogni auto i ritorni di fiamma, necessari alle fiamme del nitro. Disattivato, lascia invariate le impostazioni del gioco. Ha effetto dopo il riavvio.",
  "ニトロの排気の火に必要なアフターファイアを全車で有効にします。オフにするとゲーム本来の設定のままです。再起動後に反映されます。",
  "니트로 배기 화염에 필요한 백파이어를 모든 차에서 활성화합니다. 끄면 게임 자체 설정을 그대로 둡니다. 다시 시작한 후에 적용됩니다.",
  "讓所有車輛都能回火，這是 NOS 排氣噴火所需要的。關閉時保留遊戲本身的設定。重新啟動後生效。",
  "Pozwala każdemu autu strzelać z wydechu, czego potrzebują płomienie nitro. Wyłączone zostawia ustawienia samej gry. Działa po ponownym uruchomieniu.",
  "Разрешает выстрелы из выхлопа всем машинам — они нужны для пламени нитро. Если выключено, настройки самой игры не меняются. Действует после перезапуска.",
  "Laat elke auto knallen uit de uitlaat, wat de nitrovlammen nodig hebben. Uit laat de instellingen van het spel zelf ongemoeid. Werkt na een herstart."}},
{"Refill Nitro on Race Start",
 {"Recarregar nitro no início da corrida",
  "Nitro beim Rennstart auffüllen",
  "Remplir la nitro au départ",
  "Rellenar nitro al empezar la carrera",
  "Rellenar nitro al iniciar la carrera",
  "Ricarica il nitro alla partenza",
  "レース開始時にニトロを満タンに",
  "레이스 시작 시 니트로 충전",
  "比賽開始時補滿 NOS",
  "Uzupełnij nitro na starcie wyścigu",
  "Заправлять нитро на старте гонки",
  "Nitro bijvullen bij racestart"}},
{"Fills the nitro tank when a race starts, you quit one, you change car or you fast travel.",
 {"Enche o tanque de nitro quando uma corrida começa, quando você sai de uma, troca de carro ou faz viagem rápida.",
  "Füllt den Nitro-Tank, wenn ein Rennen startet, du eines verlässt, das Auto wechselst oder eine Schnellreise machst.",
  "Remplit le réservoir de nitro au départ d'une course, quand vous en quittez une, changez de voiture ou faites un voyage instantané.",
  "Llena el depósito de nitro al empezar una carrera, al abandonarla, al cambiar de coche o al hacer un viaje rápido.",
  "Llena el tanque de nitro al iniciar una carrera, al abandonarla, al cambiar de auto o al hacer un viaje rápido.",
  "Riempie il serbatoio del nitro quando parte una gara, quando ne esci, cambi auto o usi il trasferimento rapido.",
  "レースの開始時、レースをやめた時、車を変えた時、ファスト トラベルした時にニトロタンクを満タンにします。",
  "레이스가 시작될 때, 레이스를 그만둘 때, 차를 바꿀 때, 빠른 이동할 때 니트로 탱크를 채웁니다.",
  "比賽開始、退出比賽、更換車輛或快速移動時，將 NOS 補滿。",
  "Napełnia zbiornik nitro, gdy wyścig się zaczyna, gdy go opuszczasz, zmieniasz auto lub używasz szybkiej jazdy.",
  "Заполняет бак нитро, когда начинается гонка, вы выходите из неё, меняете машину или используете быстрое перемещение.",
  "Vult de nitrotank wanneer een race start, je er een verlaat, van auto wisselt of snel reist."}},
{"Refund Nitro on Rewind",
 {"Devolver nitro ao retroceder",
  "Nitro beim Zurückspulen erstatten",
  "Rendre la nitro au rembobinage",
  "Devolver nitro al rebobinar",
  "Devolver nitro al rebobinar",
  "Restituisci il nitro al riavvolgimento",
  "リワインドでニトロを戻す",
  "되돌리기 시 니트로 복구",
  "倒轉時返還 NOS",
  "Zwrot nitro przy przewijaniu",
  "Возврат нитро при перемотке",
  "Nitro terug bij terugspoelen"}},
{"Rewinding gives back the nitro you used since the point you rewind to.",
 {"Ao retroceder, você recebe de volta o nitro usado desde o ponto para onde voltou.",
  "Beim Zurückspulen bekommst du das Nitro zurück, das du seit dem Zielpunkt verbraucht hast.",
  "Le rembobinage rend la nitro utilisée depuis le moment où vous revenez.",
  "Al rebobinar recuperas el nitro gastado desde el punto al que vuelves.",
  "Al rebobinar recuperas el nitro gastado desde el punto al que vuelves.",
  "Riavvolgendo recuperi il nitro usato dal punto a cui torni.",
  "リワインドすると、戻った地点以降に使ったニトロが戻ります。",
  "되돌리기를 하면 되돌아간 지점 이후에 쓴 니트로를 돌려받습니다.",
  "倒轉時，會返還自倒轉點之後用掉的 NOS。",
  "Przewijanie oddaje nitro zużyte od miejsca, do którego wracasz.",
  "При перемотке возвращается нитро, потраченное с того момента, к которому вы вернулись.",
  "Terugspoelen geeft de nitro terug die je sinds dat punt hebt gebruikt."}},
{"Nitro Controller Rumble",
 {"Vibração do controle no nitro",
  "Controller-Vibration bei Nitro",
  "Vibration de la manette (nitro)",
  "Vibración del mando con nitro",
  "Vibración del control con nitro",
  "Vibrazione del controller col nitro",
  "ニトロ時のコントローラー振動",
  "니트로 컨트롤러 진동",
  "NOS 控制器震動",
  "Wibracje kontrolera przy nitro",
  "Вибрация контроллера при нитро",
  "Controllervibratie bij nitro"}},
{"Rumbles the controller while Nitro is active. The game's own rumble isn't affected.",
 {"Faz o controle vibrar enquanto o nitro está ativo. A vibração do próprio jogo não é afetada.",
  "Lässt den Controller vibrieren, solange Nitro aktiv ist. Die Vibration des Spiels bleibt unverändert.",
  "Fait vibrer la manette pendant la nitro. Les vibrations du jeu ne changent pas.",
  "Hace vibrar el mando mientras el nitro está activo. La vibración del propio juego no cambia.",
  "Hace vibrar el control mientras el nitro está activo. La vibración del propio juego no cambia.",
  "Fa vibrare il controller mentre il nitro è attivo. La vibrazione del gioco non cambia.",
  "ニトロ使用中にコントローラーを振動させます。ゲーム本来の振動には影響しません。",
  "니트로 사용 중 컨트롤러를 진동시킵니다. 게임 자체 진동에는 영향이 없습니다.",
  "NOS 啟動時讓控制器震動。不影響遊戲本身的震動。",
  "Kontroler wibruje, gdy nitro jest aktywne. Wibracje samej gry pozostają bez zmian.",
  "Контроллер вибрирует, пока работает нитро. Вибрация самой игры не меняется.",
  "Laat de controller trillen zolang nitro actief is. De vibratie van het spel zelf verandert niet."}},
{"Drivatar Nitro",
 {"Nitro dos Drivatars",
  "Drivatar-Nitro",
  "Nitro des Drivatars",
  "Nitro de los Drivatars",
  "Nitro de los Drivatars",
  "Nitro dei Drivatar",
  "Drivatarのニトロ",
  "Drivatar 니트로",
  "Drivatar NOS",
  "Nitro Drivatarów",
  "Нитро аватаров гонщиков",
  "Drivatar-nitro"}},
{"Drivatars use nitro too, whenever they can (not before the race starts), from a tank with twice your capacity and recharge, with flames and sound. Not once they're over 100 m ahead of you, so you can catch up. Applies right away.",
 {"Os Drivatars também usam nitro sempre que podem (nunca antes da largada), com um tanque de capacidade e recarga duas vezes maiores que as suas, com chamas e som. Não quando estão mais de 100 m à sua frente, para você poder alcançá-los. Vale na hora.",
  "Auch Drivatars nutzen Nitro, wann immer sie können (nicht vor dem Rennstart), aus einem Tank mit doppelter Kapazität und Aufladung, mit Flammen und Sound. Nicht, wenn sie über 100 m vor dir liegen, damit du aufholen kannst. Wirkt sofort.",
  "Les Drivatars utilisent aussi la nitro dès qu'ils le peuvent (jamais avant le départ), avec un réservoir deux fois plus grand et une recharge deux fois plus rapide que les vôtres, flammes et son compris. Pas quand ils ont plus de 100 m d'avance sur vous, pour que vous puissiez les rattraper. Effet immédiat.",
  "Los Drivatars también usan nitro siempre que pueden (nunca antes de la salida), con un depósito del doble de capacidad y recarga que el tuyo, con llamas y sonido. No cuando te sacan más de 100 m, para que puedas alcanzarlos. Se aplica al momento.",
  "Los Drivatars también usan nitro siempre que pueden (nunca antes de la salida), con un tanque del doble de capacidad y recarga que el tuyo, con llamas y sonido. No cuando te llevan más de 100 m de ventaja, para que puedas alcanzarlos. Se aplica de inmediato.",
  "Anche i Drivatar usano il nitro quando possono (mai prima della partenza), con un serbatoio dalla capacità e dalla ricarica doppie rispetto alle tue, con fiamme e suono. Non quando sono più di 100 m davanti a te, così puoi raggiungerli. Effetto immediato.",
  "Drivatarも使えるときはいつでもニトロを使います（レース開始前は除く）。タンク容量と回復量はプレイヤーの2倍で、火と音も出ます。100 m以上リードしているときは使わないので、追いつけます。すぐに反映されます。",
  "Drivatar도 쓸 수 있을 때마다 니트로를 씁니다(레이스 시작 전 제외). 탱크 용량과 충전량은 플레이어의 2배이며 화염과 소리도 납니다. 100 m 이상 앞서 있을 때는 쓰지 않으므로 따라잡을 수 있습니다. 바로 적용됩니다.",
  "Drivatar 也會盡可能使用 NOS (比賽開始前除外)，容量與回充速度是你的兩倍，並有噴火與音效。領先你超過 100 公尺時不會使用，讓你能夠追上。立即生效。",
  "Drivatary też używają nitro, kiedy tylko mogą (nie przed startem wyścigu), ze zbiornika o dwa razy większej pojemności i szybszym ładowaniu niż twój, z płomieniami i dźwiękiem. Nie wtedy, gdy są ponad 100 m przed tobą, więc możesz je dogonić. Działa od razu.",
  "Аватары гонщиков тоже используют нитро, когда могут (но не до старта гонки): бак у них вдвое больше вашего и заправляется вдвое быстрее, с пламенем и звуком. Но не когда они впереди вас больше чем на 100 м, чтобы вы могли их догнать. Действует сразу.",
  "Ook Drivatars gebruiken nitro wanneer ze kunnen (niet voor de start), uit een tank met twee keer jouw capaciteit en bijvulsnelheid, met vlammen en geluid. Niet als ze meer dan 100 m voor je liggen, zodat je ze kunt inhalen. Werkt meteen."}},
{"Nitro Button",
 {"Botão do nitro",
  "Nitro-Taste",
  "Bouton de nitro",
  "Botón de nitro",
  "Botón de nitro",
  "Pulsante del nitro",
  "ニトロボタン",
  "니트로 버튼",
  "NOS 按鈕",
  "Przycisk nitro",
  "Кнопка нитро",
  "Nitroknop"}},
{"Which button burns nitro.",
 {"O botão que usa o nitro.",
  "Die Taste, mit der Nitro gezündet wird.",
  "Le bouton qui déclenche la nitro.",
  "El botón que activa el nitro.",
  "El botón que activa el nitro.",
  "Il pulsante che attiva il nitro.",
  "ニトロを使うボタンです。",
  "니트로를 쓰는 버튼입니다.",
  "使用 NOS 的按鈕。",
  "Przycisk, który odpala nitro.",
  "Кнопка, включающая нитро.",
  "De knop die nitro gebruikt."}},
{"Pick one the game doesn't use while driving. Applies right away.",
 {"Escolha um que o jogo não use ao dirigir. Vale na hora.",
  "Wähle eine, die das Spiel beim Fahren nicht nutzt. Wirkt sofort.",
  "Choisissez-en un que le jeu n'utilise pas en conduite. Effet immédiat.",
  "Elige uno que el juego no use al conducir. Se aplica al momento.",
  "Elige uno que el juego no use al manejar. Se aplica de inmediato.",
  "Scegline uno che il gioco non usa durante la guida. Effetto immediato.",
  "走行中にゲームが使わないボタンを選んでください。すぐに反映されます。",
  "주행 중 게임이 쓰지 않는 버튼을 고르세요. 바로 적용됩니다.",
  "請選擇遊戲在駕駛時不使用的按鈕。立即生效。",
  "Wybierz taki, którego gra nie używa podczas jazdy. Działa od razu.",
  "Выберите ту, что игра не использует во время езды. Действует сразу.",
  "Kies er een die het spel tijdens het rijden niet gebruikt. Werkt meteen."}},
{"HUD Theme",
 {"Tema da tela do jogo",
  "HUD-Design",
  "Thème de l'ATH",
  "Tema del HUD",
  "Tema del HUD",
  "Tema dell'HUD",
  "画面表示のテーマ",
  "HUD 테마",
  "抬頭顯示器主題",
  "Motyw ekranu",
  "Тема приборной панели",
  "Thema scherminfo"}},
{"Original: the game's HUD with a yellow NOS bar. Fast & Furious: cyan NOS bar, gears and assist icons.",
 {"Original: a tela do jogo com barra de NOS amarela. Fast & Furious: barra de NOS, marchas e ícones de assistência em ciano.",
  "Original: das HUD des Spiels mit gelbem NOS-Balken. Fast & Furious: NOS-Balken, Gänge und Assistenz-Symbole in Cyan.",
  "Original : l'ATH du jeu avec une barre de NOS jaune. Fast & Furious : barre de NOS, rapports et icônes d'aides en cyan.",
  "Original: el HUD del juego con una barra de NOS amarilla. Fast & Furious: barra de NOS, marchas e iconos de asistencias en cian.",
  "Original: el HUD del juego con una barra de NOS amarilla. Fast & Furious: barra de NOS, marchas e iconos de asistencias en cian.",
  "Originale: l'HUD del gioco con la barra NOS gialla. Fast & Furious: barra NOS, marce e icone degli aiuti in ciano.",
  "オリジナルはゲーム本来の画面表示で、NOSバーは黄色です。Fast & FuriousはNOSバー、ギア、アシストのアイコンがシアンです。",
  "오리지널: 노란색 NOS 바가 있는 게임 기본 HUD. Fast & Furious: 청록색 NOS 바, 기어, 보조 아이콘.",
  "原始：遊戲本身的抬頭顯示器，NOS 條為黃色。Fast & Furious：NOS 條、檔位與輔助圖示為青色。",
  "Oryginalny: ekran gry z żółtym paskiem NOS. Fast & Furious: pasek NOS, biegi i ikony asyst w kolorze cyjan.",
  "Оригинал: приборная панель игры с жёлтой полосой NOS. Fast & Furious: полоса NOS, передачи и значки помощников голубого цвета.",
  "Origineel: de scherminfo van het spel met een gele NOS-balk. Fast & Furious: NOS-balk, versnellingen en hulpiconen in cyaan."}},
{"1 Original (yellow NOS bar), 2 Fast & Furious (cyan NOS bar, gears and assist icons).",
 {"1 Original (barra de NOS amarela), 2 Fast & Furious (barra de NOS, marchas e ícones de assistência em ciano).",
  "1 Original (gelber NOS-Balken), 2 Fast & Furious (NOS-Balken, Gänge und Assistenz-Symbole in Cyan).",
  "1 Original (barre de NOS jaune), 2 Fast & Furious (barre de NOS, rapports et icônes d'aides en cyan).",
  "1 Original (barra de NOS amarilla), 2 Fast & Furious (barra de NOS, marchas e iconos de asistencias en cian).",
  "1 Original (barra de NOS amarilla), 2 Fast & Furious (barra de NOS, marchas e iconos de asistencias en cian).",
  "1 Originale (barra NOS gialla), 2 Fast & Furious (barra NOS, marce e icone degli aiuti in ciano).",
  "1 オリジナル（NOSバーは黄色）、2 Fast & Furious（NOSバー、ギア、アシストのアイコンがシアン）。",
  "1 오리지널(노란색 NOS 바), 2 Fast & Furious(청록색 NOS 바, 기어, 보조 아이콘).",
  "1 原始 (NOS 條為黃色)，2 Fast & Furious (NOS 條、檔位與輔助圖示為青色)。",
  "1 Oryginalny (żółty pasek NOS), 2 Fast & Furious (pasek NOS, biegi i ikony asyst w kolorze cyjan).",
  "1 Оригинал (жёлтая полоса NOS), 2 Fast & Furious (полоса NOS, передачи и значки помощников голубого цвета).",
  "1 Origineel (gele NOS-balk), 2 Fast & Furious (NOS-balk, versnellingen en hulpiconen in cyaan)."}},
{"Original",
 {"Original",
  "Original",
  "Original",
  "Original",
  "Original",
  "Originale",
  "オリジナル",
  "오리지널",
  "原始",
  "Oryginalny",
  "Оригинал",
  "Origineel"}},
{"Nitro Boost Multiplier",
 {"Multiplicador do nitro",
  "Nitro-Boost-Faktor",
  "Multiplicateur de nitro",
  "Multiplicador del nitro",
  "Multiplicador del nitro",
  "Moltiplicatore del nitro",
  "ニトロのブースト倍率",
  "니트로 부스트 배율",
  "NOS 加速倍率",
  "Mnożnik nitro",
  "Множитель нитро",
  "Nitro-boostfactor"}},
{"Multiplies forward acceleration while Nitro is active (Spire default: 1.6x).",
 {"Multiplica a aceleração para a frente enquanto o nitro está ativo (padrão do Spire: 1,6x).",
  "Vervielfacht die Beschleunigung nach vorn, solange Nitro aktiv ist (Spire-Standard: 1,6x).",
  "Multiplie l'accélération vers l'avant pendant la nitro (valeur de Spire : 1,6x).",
  "Multiplica la aceleración hacia delante mientras el nitro está activo (valor de Spire: 1,6x).",
  "Multiplica la aceleración hacia adelante mientras el nitro está activo (valor de Spire: 1.6x).",
  "Moltiplica l'accelerazione in avanti mentre il nitro è attivo (valore di Spire: 1,6x).",
  "ニトロ使用中の前方への加速を倍増します（Spireの初期値は1.6倍）。",
  "니트로 사용 중 전진 가속을 배로 늘립니다(Spire 기본값: 1.6배).",
  "NOS 啟動時，前進加速度乘以此倍率 (Spire 預設值：1.6 倍)。",
  "Mnoży przyspieszenie do przodu, gdy nitro jest aktywne (domyślnie w Spire: 1,6x).",
  "Умножает ускорение вперёд, пока работает нитро (по умолчанию в Spire: 1,6x).",
  "Vermenigvuldigt de acceleratie naar voren zolang nitro actief is (standaard in Spire: 1,6x)."}},
{"Nitro Tank Capacity (sec)",
 {"Capacidade do tanque de nitro (s)",
  "Nitro-Tankinhalt (s)",
  "Capacité du réservoir de nitro (s)",
  "Capacidad del depósito de nitro (s)",
  "Capacidad del tanque de nitro (s)",
  "Capacità del serbatoio del nitro (s)",
  "ニトロタンク容量（秒）",
  "니트로 탱크 용량(초)",
  "NOS 容量 (秒)",
  "Pojemność zbiornika nitro (s)",
  "Объём бака нитро (с)",
  "Inhoud nitrotank (s)"}},
{"Total seconds of continuous boost available in a full tank (Spire default: 20s).",
 {"Segundos de nitro contínuo em um tanque cheio (padrão do Spire: 20 s).",
  "Sekunden Dauer-Boost in einem vollen Tank (Spire-Standard: 20 s).",
  "Secondes de boost continu dans un réservoir plein (valeur de Spire : 20 s).",
  "Segundos de nitro continuo con el depósito lleno (valor de Spire: 20 s).",
  "Segundos de nitro continuo con el tanque lleno (valor de Spire: 20 s).",
  "Secondi di boost continuo con il serbatoio pieno (valore di Spire: 20 s).",
  "満タン時に連続でブーストできる秒数です（Spireの初期値は20秒）。",
  "가득 찬 탱크로 계속 부스트할 수 있는 초입니다(Spire 기본값: 20초).",
  "補滿時可連續加速的秒數 (Spire 預設值：20 秒)。",
  "Liczba sekund ciągłego przyspieszania przy pełnym zbiorniku (domyślnie w Spire: 20 s).",
  "Сколько секунд непрерывного ускорения даёт полный бак (по умолчанию в Spire: 20 с).",
  "Seconden onafgebroken boost in een volle tank (standaard in Spire: 20 s)."}},
{"Nitro Buildup Time (sec)",
 {"Tempo de entrada do nitro (s)",
  "Nitro-Aufbauzeit (s)",
  "Montée en puissance de la nitro (s)",
  "Tiempo de entrada del nitro (s)",
  "Tiempo de entrada del nitro (s)",
  "Tempo di entrata del nitro (s)",
  "ニトロの立ち上がり時間（秒）",
  "니트로 상승 시간(초)",
  "NOS 啟動時間 (秒)",
  "Czas narastania nitro (s)",
  "Время разгона нитро (с)",
  "Opbouwtijd nitro (s)"}},
{"Time taken to ramp up to full boost torque (Spire default: 0.5s).",
 {"Tempo até chegar à força total do nitro (padrão do Spire: 0,5 s).",
  "Zeit bis zur vollen Boost-Kraft (Spire-Standard: 0,5 s).",
  "Temps nécessaire pour atteindre la pleine poussée (valeur de Spire : 0,5 s).",
  "Tiempo hasta alcanzar la fuerza máxima del nitro (valor de Spire: 0,5 s).",
  "Tiempo hasta alcanzar la fuerza máxima del nitro (valor de Spire: 0.5 s).",
  "Tempo per arrivare alla piena spinta del nitro (valore di Spire: 0,5 s).",
  "ブーストが最大になるまでの時間です（Spireの初期値は0.5秒）。",
  "부스트가 최대에 이를 때까지의 시간입니다(Spire 기본값: 0.5초).",
  "達到最大加速所需的時間 (Spire 預設值：0.5 秒)。",
  "Czas dojścia do pełnej siły nitro (domyślnie w Spire: 0,5 s).",
  "Время, за которое нитро выходит на полную силу (по умолчанию в Spire: 0,5 с).",
  "Tijd tot de volle boostkracht (standaard in Spire: 0,5 s)."}},
{"Nitro Recharge Rate",
 {"Recarga do nitro",
  "Nitro-Aufladerate",
  "Recharge de la nitro",
  "Recarga del nitro",
  "Recarga del nitro",
  "Ricarica del nitro",
  "ニトロの回復速度",
  "니트로 충전 속도",
  "NOS 回充速度",
  "Szybkość ładowania nitro",
  "Скорость восполнения нитро",
  "Nitro-bijvulsnelheid"}},
{"Seconds of nitro restored per second while not boosting.",
 {"Segundos de nitro recuperados por segundo sem usar o nitro.",
  "Sekunden Nitro, die pro Sekunde ohne Boost zurückkommen.",
  "Secondes de nitro récupérées par seconde hors boost.",
  "Segundos de nitro que se recuperan por segundo sin usarlo.",
  "Segundos de nitro que se recuperan por segundo sin usarlo.",
  "Secondi di nitro recuperati al secondo quando non lo usi.",
  "ブーストしていないとき、1秒ごとに回復するニトロの秒数です。",
  "부스트하지 않을 때 1초마다 회복되는 니트로의 초입니다.",
  "未加速時，每秒回充的 NOS 秒數。",
  "Ile sekund nitro wraca na sekundę, gdy go nie używasz.",
  "Сколько секунд нитро восполняется за секунду без ускорения.",
  "Seconden nitro die per seconde terugkomen zonder boost."}},
{"Nitro Sound Volume",
 {"Volume do som do nitro",
  "Nitro-Lautstärke",
  "Volume du son de la nitro",
  "Volumen del sonido del nitro",
  "Volumen del sonido del nitro",
  "Volume del suono del nitro",
  "ニトロの音量",
  "니트로 음량",
  "NOS 音量",
  "Głośność dźwięku nitro",
  "Громкость звука нитро",
  "Volume nitrogeluid"}},
{"Volume of the boost and stop sounds. The game's own volume sliders don't change them.",
 {"Volume dos sons do nitro. Os controles de volume do próprio jogo não os alteram.",
  "Lautstärke der Boost- und Stopp-Geräusche. Die Lautstärkeregler des Spiels ändern sie nicht.",
  "Volume des sons de boost et d'arrêt. Les réglages de volume du jeu ne les modifient pas.",
  "Volumen de los sonidos del nitro. Los controles de volumen del propio juego no los cambian.",
  "Volumen de los sonidos del nitro. Los controles de volumen del propio juego no los cambian.",
  "Volume dei suoni del boost e dell'arresto. I cursori del volume del gioco non li cambiano.",
  "ブーストと停止の音量です。ゲーム本来の音量設定では変わりません。",
  "부스트와 정지 소리의 음량입니다. 게임 자체 음량 설정으로는 달라지지 않습니다.",
  "加速與停止音效的音量。遊戲本身的音量設定不會改變它們。",
  "Głośność dźwięków nitro. Suwaki głośności samej gry ich nie zmieniają.",
  "Громкость звуков ускорения и его окончания. Ползунки громкости самой игры на них не влияют.",
  "Volume van de boost- en stopgeluiden. De volumeschuiven van het spel zelf veranderen ze niet."}},
{"Drivatar Nitro Volume",
 {"Volume do nitro dos Drivatars",
  "Drivatar-Nitro-Lautstärke",
  "Volume de la nitro des Drivatars",
  "Volumen del nitro de los Drivatars",
  "Volumen del nitro de los Drivatars",
  "Volume del nitro dei Drivatar",
  "Drivatarのニトロ音量",
  "Drivatar 니트로 음량",
  "Drivatar NOS 音量",
  "Głośność nitro Drivatarów",
  "Громкость нитро аватаров гонщиков",
  "Volume Drivatar-nitro"}},
{"Volume of the Drivatars' nitro, heard from their cars (with Drivatar Nitro on). 0 silences it.",
 {"Volume do nitro dos Drivatars, ouvido dos carros deles (com o Nitro dos Drivatars ligado). 0 silencia.",
  "Lautstärke des Drivatar-Nitros, von ihren Autos aus zu hören (mit Drivatar-Nitro an). 0 schaltet es stumm.",
  "Volume de la nitro des Drivatars, entendue depuis leurs voitures (avec Nitro des Drivatars activé). 0 la coupe.",
  "Volumen del nitro de los Drivatars, que se oye desde sus coches (con Nitro de los Drivatars activado). 0 lo silencia.",
  "Volumen del nitro de los Drivatars, que se oye desde sus autos (con Nitro de los Drivatars activado). 0 lo silencia.",
  "Volume del nitro dei Drivatar, udito dalle loro auto (con Nitro dei Drivatar attivo). 0 lo silenzia.",
  "Drivatarのニトロの音量で、その車から聞こえます（Drivatarのニトロがオンのとき）。0で無音になります。",
  "Drivatar 니트로의 음량으로, 그 차에서 들립니다(Drivatar 니트로가 켜져 있을 때). 0이면 소리가 나지 않습니다.",
  "Drivatar NOS 的音量，從他們的車輛發出 (需開啟 Drivatar NOS)。0 為靜音。",
  "Głośność nitro Drivatarów, słyszanego z ich aut (gdy Nitro Drivatarów jest włączone). 0 je wycisza.",
  "Громкость нитро аватаров гонщиков, слышного от их машин (при включённом нитро аватаров). 0 — без звука.",
  "Volume van de nitro van Drivatars, te horen vanuit hun auto's (met Drivatar-nitro aan). 0 zet het uit."}},
{"Nitro Camera Shake",
 {"Tremor da câmera no nitro",
  "Kamerawackeln bei Nitro",
  "Tremblement de caméra (nitro)",
  "Vibración de cámara con nitro",
  "Vibración de cámara con nitro",
  "Tremolio della visuale col nitro",
  "ニトロ時のカメラの揺れ",
  "니트로 카메라 흔들림",
  "NOS 視角搖動",
  "Drgania kamery przy nitro",
  "Тряска камеры при нитро",
  "Camerabeweging bij nitro"}},
{"How much the camera shakes while Nitro is active, in the driver, hood, bumper and chase views. 0 turns it off.",
 {"Quanto a câmera treme com o nitro ativo, nas visões do piloto, capô, para-choque e perseguição. 0 desliga.",
  "Wie stark die Kamera bei aktivem Nitro wackelt, in Fahrer-, Motorhauben-, Stoßstangen- und Verfolgeransicht. 0 schaltet es ab.",
  "Intensité du tremblement de la caméra pendant la nitro, en vues pilote, capot, pare-chocs et poursuite. 0 le désactive.",
  "Cuánto tiembla la cámara con el nitro activo, en las vistas de piloto, capó, parachoques y persecución. 0 lo desactiva.",
  "Cuánto tiembla la cámara con el nitro activo, en las vistas de conductor, capó, parachoques y persecución. 0 lo desactiva.",
  "Quanto trema la visuale col nitro attivo, nelle visuali pilota, cofano, paraurti e inseguimento. 0 lo disattiva.",
  "ニトロ使用中のカメラの揺れの強さです（ドライバー、ボンネット、バンパー、追従の各視点）。0でオフになります。",
  "니트로 사용 중 카메라가 흔들리는 정도입니다(드라이버, Bonnet, 범퍼, 추적 시점). 0이면 꺼집니다.",
  "NOS 啟動時視角搖動的程度，適用於車手、引擎蓋、保險桿與追車視角。0 為關閉。",
  "Jak mocno trzęsie się kamera, gdy nitro jest aktywne, w widokach kierowcy, maski, zderzaka i zza auta. 0 wyłącza.",
  "Насколько трясётся камера при нитро в видах от водителя, с капота, с бампера и сзади. 0 — выключено.",
  "Hoe sterk de camera beweegt bij nitro, in de coureur-, bonnet-, bumper- en achtervolgcamera. 0 zet het uit."}},
};
// clang-format on

wchar_t g_ini[MAX_PATH] = {};
std::atomic<int> g_lang{-1};   // index into kCodes, -1 English: the one tr() uses (the one seen last time)

// The game's text language: an index into kCodes, -1 English, -2 not known yet.
int game_language(const void* game) {
    __try {
        const uint8_t* manager = *reinterpret_cast<uint8_t* const*>(static_cast<const uint8_t*>(game) + kStringsManager);
        if (!manager) return -2;
        const uint8_t* name = manager + 0x30;
        size_t count, capacity;
        std::memcpy(&count, name + 16, sizeof count);
        std::memcpy(&capacity, name + 24, sizeof capacity);
        if (capacity != 15 || count < 2 || count > 3 || name[count]) return -2;   // a short string, as codes are
        for (int i = 0; i < kLangs; ++i)
            if (!_stricmp(reinterpret_cast<const char*>(name), kCodes[i])) return i;
        return -1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -2;
    }
}

int code_index(const wchar_t* code) {
    for (int i = 0; i < kLangs; ++i) {
        wchar_t w[8] = {};
        MultiByteToWideChar(CP_UTF8, 0, kCodes[i], -1, w, 8);
        if (!_wcsicmp(code, w)) return i;
    }
    return -1;
}

// A locale ("ja-JP", "pt-BR", "es-MX"...) as the game's text language: an index into kCodes, -1 English, -2 one the
// game has no text for.
int locale_language(const wchar_t* locale) {
    wchar_t lang[4] = {}, region[8] = {};
    swscanf_s(locale, L"%3[a-zA-Z]-%7s", lang, (unsigned)_countof(lang), region, (unsigned)_countof(region));
    auto is = [&](const wchar_t* l) { return !_wcsicmp(lang, l); };
    const wchar_t* code = nullptr;
    if (is(L"en") || is(L"cs") || is(L"hu")) return -1;   // Czech and Hungarian: the game's text is English there
    if (is(L"pt")) code = L"BR";
    else if (is(L"es")) code = (!_wcsicmp(region, L"ES") || !region[0]) ? L"ES" : L"MX";   // Latin America: the Mexican text
    else if (is(L"zh")) code = (!_wcsicmp(region, L"TW") || !_wcsicmp(region, L"HK") || !_wcsicmp(region, L"MO") ||
                                !_wcsnicmp(region, L"Hant", 4)) ? L"CHT" : nullptr;
    else if (is(L"ja")) code = L"JP";
    else if (is(L"ko")) code = L"KO";
    else if (is(L"de")) code = L"DE";
    else if (is(L"fr")) code = L"FR";
    else if (is(L"it")) code = L"IT";
    else if (is(L"pl")) code = L"PL";
    else if (is(L"ru")) code = L"RU";
    else if (is(L"nl")) code = L"NL";
    return code ? code_index(code) : -2;
}

// The language Azimuth starts the game in: its data\azimuth.ini, [launcher] game = the title it runs, whose section has
// language = a locale, or "system" for Windows' own. -2 when it can't be told.
int azimuth_language(const wchar_t* azimuth_dir) {
    if (!azimuth_dir || !azimuth_dir[0]) return -2;
    wchar_t ini[MAX_PATH], title[32] = {}, section[48], locale[LOCALE_NAME_MAX_LENGTH] = {};
    swprintf_s(ini, L"%s\\data\\azimuth.ini", azimuth_dir);
    if (!GetPrivateProfileStringW(L"launcher", L"game", L"", title, _countof(title), ini)) return -2;
    swprintf_s(section, L"title-%s", title);
    if (!GetPrivateProfileStringW(section, L"language", L"", locale, _countof(locale), ini)) return -2;
    if (!_wcsicmp(locale, L"system") && !LCIDToLocaleName(MAKELCID(GetUserDefaultUILanguage(), SORT_DEFAULT), locale,
                                                          _countof(locale), 0))
        return -2;
    return locale_language(locale);
}

} // namespace

void lang_init(const wchar_t* ini, const wchar_t* azimuth_dir) {
    wcsncpy_s(g_ini, ini, _TRUNCATE);
    // The language the game had last time: it keeps its own text-language option, so that's the best guess (Azimuth's
    // locale needn't be the game's text language, and Azimuth saves a changed one only after the game has started).
    // Azimuth's locale only before the game has ever been seen, then English.
    wchar_t code[8] = {};
    int lang;
    if (GetPrivateProfileStringW(L"Language", L"Last", L"", code, 8, g_ini))
        lang = code_index(code);
    else if ((lang = azimuth_language(azimuth_dir)) == -2)
        lang = -1;
    g_lang.store(lang);
}

void lang_poll(const void* game) {
    static int saved = -3;
    const int now = game ? game_language(game) : -2;
    if (now == -2 || now == saved) return;
    saved = now;
    if (!g_ini[0]) return;
    wchar_t code[8] = L"EN";
    if (now >= 0) MultiByteToWideChar(CP_UTF8, 0, kCodes[now], -1, code, 8);
    WritePrivateProfileStringW(L"Language", L"Last", code, g_ini);   // the Mods page's language next time
}

const char* tr(const char* english) {
    const int l = g_lang.load();
    if (!english || l < 0) return english;
    for (const Text& t : kTexts)
        if (!std::strcmp(t.en, english)) return t.t[l];
    return english;
}

const char* lang_code() {
    const int l = g_lang.load();
    return l < 0 ? "EN" : kCodes[l];
}
