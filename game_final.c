buiduyvuong_20223832@20223832:~$ g++ game.cpp -o sudoku_game -lsfml-graphics -lsfml-window -lsfml-system
buiduyvuong_20223832@20223832:~$ ./sudoku_game
buiduyvuong_20223832@20223832:~$ cat game.cpp
#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>
#include <array>
#include <string>
#include <chrono>
#include <algorithm>
#include <random>
#include <iomanip>
#include <sstream>

const int CELL_SIZE = 60;     
const int GRID_OFFSET = 40;   
const int BOARD_SIZE = CELL_SIZE * 9;
const int WINDOW_WIDTH = BOARD_SIZE + GRID_OFFSET * 2;       // 620
const int WINDOW_HEIGHT = BOARD_SIZE + GRID_OFFSET * 2 + 160; // 780

// Thêm CHALLENGE_TYPE_SELECT (chọn Loại 1/Loại 2) trước khi vào CHALLENGE_SELECT (chọn cấp độ)
enum class GameMode { 
    MAIN_MENU, 
    CHALLENGE_TYPE_SELECT,
    CHALLENGE_SELECT, 
    PLAYING_PRACTICE, 
    PLAYING_CHALLENGE, 
    GAME_OVER, 
    VICTORY, 
    HISTORY_MENU 
};

struct HistoryEntry {
    std::string modeName;
    int duration;
    int wrongMoves;
    bool isWin;
    int level; 
    int challengeType; // 1 = dem loi, 2 = chi gioi han thoi gian (0 = khong ap dung, vi du Luyen Tap)
};

// ═══════════════════════════════════════════
//  Class SudokuGenerator (Thuật toán lõi)
// ═══════════════════════════════════════════
class SudokuGenerator {
    std::mt19937 rng;
    using Grid = std::array<std::array<int,9>,9>;

    bool valid(const Grid& b, int r, int c, int n) const {
        for (int i=0; i<9; i++) { if(b[r][i]==n || b[i][c]==n) return false; }
        int br=r/3*3, bc=c/3*3;
        for (int i=0; i<3; i++) for(int j=0; j<3; j++) if(b[br+i][bc+j]==n) return false;
        return true;
    }

    bool fillRandom(Grid& b) {
        for (int r=0; r<9; r++) for(int c=0; c<9; c++) if(b[r][c]==0) {
            std::vector<int> ns={1,2,3,4,5,6,7,8,9};
            std::shuffle(ns.begin(), ns.end(), rng);
            for (int n:ns) { if(valid(b,r,c,n)){ b[r][c]=n; if(fillRandom(b)) return true; b[r][c]=0; } }
            return false;
        }
        return true;
    }

    void countSol(Grid b, int& cnt) {
        if (cnt >= 2) return;
        for (int r=0; r<9; r++) for(int c=0; c<9; c++) if(b[r][c]==0) {
            for (int n=1; n<=9; n++) if(valid(b,r,c,n)) { b[r][c]=n; countSol(b,cnt); if(cnt>=2) return; }
            return;
        }
        cnt++;
    }

public:
    SudokuGenerator() : rng(std::chrono::steady_clock::now().time_since_epoch().count()) {}

    Grid generateFull() {
        Grid b{}; for(auto& r:b) r.fill(0);
        fillRandom(b); return b;
    }

    Grid generateMaxPuzzle(const Grid& full) {
        Grid p = full;
        std::vector<std::pair<int,int>> cells;
        for(int r=0; r<9; r++) for(int c=0; c<9; c++) cells.push_back({r,c});
        std::shuffle(cells.begin(), cells.end(), rng);

        for (auto& [r,c]: cells) {
            int bak = p[r][c]; p[r][c] = 0;
            int cnt = 0; countSol(p, cnt);
            if (cnt != 1) p[r][c] = bak; 
        }
        return p;
    }
};

// ═══════════════════════════════════════════
//  Class SudokuBoard (Quản lý trạng thái)
// ═══════════════════════════════════════════
class SudokuBoard {
public:
    using Grid = std::array<std::array<int,9>,9>;
    Grid puzzle, solution, player;
    std::array<std::array<bool,9>,9> err;

    SudokuBoard() { reset(); }

    void reset() {
        for(auto& r:puzzle)   r.fill(0);
        for(auto& r:solution) r.fill(0);
        for(auto& r:player)   r.fill(0);
        for(auto& r:err)      r.fill(false);
    }

    bool isFixed(int r, int c) const { return puzzle[r][c] != 0; }
    int  get(int r, int c)     const { return isFixed(r,c) ? puzzle[r][c] : player[r][c]; }

    bool set(int r, int c, int v) {
        if(isFixed(r,c)) return true;
        player[r][c] = v;
        if(v == 0){ err[r][c] = false; return true; }
        err[r][c] = (v != solution[r][c]);
        return !err[r][c];
    }

    bool complete() const {
        for(int r=0; r<9; r++) for(int c=0; c<9; c++) if(get(r,c) != solution[r][c]) return false;
        return true;
    }
};

std::string formatTime(int totalSeconds) {
    if (totalSeconds < 0) return "00:00";
    int mins = totalSeconds / 60;
    int secs = totalSeconds % 60;
    std::ostringstream ss;
    ss << std::setfill('0') << std::setw(2) << mins << ":" << std::setw(2) << secs;
    return ss.str();
}

int main() {
    sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "Sudoku Lite - HUST Project");
    window.setFramerateLimit(60);

    sf::Font font;
    if (!font.loadFromFile("arial.ttf")) {
        std::cout << "Loi font chu arial.ttf!\n";
        return -1;
    }

    SudokuGenerator generator;
    SudokuBoard board;
    GameMode mode = GameMode::MAIN_MENU;
    std::vector<HistoryEntry> history;

    int selectedRow = -1, selectedCol = -1;
    int wrongMoves = 0;
    int challengeLevel = 1; 
    int challengeType = 1; // 1 = Dem loi sai theo cap do | 2 = Chi gioi han thoi gian, khong bao loi

    // Loại 1: giới hạn lượt sai theo cấp độ (giữ nguyên thời gian thoải mái hơn)
    int maxTimeByLvl_T1[]  = {2400, 2100, 1800, 1500, 1200, 900, 720, 540}; 
    int maxWrongByLvl_T1[] = {  24,   18,   14,   10,   5,   3,   1,   0}; 

    // Loại 2: KHÔNG đếm lỗi, chỉ siết thời gian theo cấp độ -
    int maxTimeByLvl_T2[]  = {1800, 1500, 1080,  900, 720, 540, 360, 270};

    sf::Clock gameClock;
    int elapsedSeconds = 0;
    int countdownSeconds = 0;
    bool isGameOverReasonTime = false;
    bool revealAnswer = false; // Chỉ true khi người chơi bấm "Xem dap an" ở màn GAME_OVER

    // UI Buttons Menu chính
    

sf::RectangleShape btnPractice(sf::Vector2f(260, 50)), btnChallengeMenu(sf::Vector2f(260, 50)), btnHistory(sf::Vector2f(260, 50));
    btnPractice.setPosition(WINDOW_WIDTH/2 - 130, 240);       btnPractice.setFillColor(sf::Color(52, 152, 219));
    btnChallengeMenu.setPosition(WINDOW_WIDTH/2 - 130, 310);  btnChallengeMenu.setFillColor(sf::Color(230, 126, 34));
    btnHistory.setPosition(WINDOW_WIDTH/2 - 130, 380);        btnHistory.setFillColor(sf::Color(149, 165, 166));

    // UI Khu vực chọn LOẠI thử thách (Loại 1 / Loại 2)
  // Thay vì kích thước (260, 90), tăng chiều rộng lên 460 để chứa vừa text
sf::RectangleShape btnType1(sf::Vector2f(460, 90)), btnType2(sf::Vector2f(460, 90));

// Cập nhật lại vị trí X: lấy WINDOW_WIDTH/2 trừ đi một nửa chiều rộng mới (230) để căn giữa
btnType1.setPosition(WINDOW_WIDTH/2 - 230, 220); btnType1.setFillColor(sf::Color(41, 128, 185));
btnType2.setPosition(WINDOW_WIDTH/2 - 230, 330); btnType2.setFillColor(sf::Color(192, 57, 43));
    // UI Khởi tạo 8 nút chọn Cấp độ (Màn hình CHALLENGE_SELECT)
    std::vector<sf::RectangleShape> btnLevels(8);
    for (int i = 0; i < 8; i++) {
        btnLevels[i].setSize(sf::Vector2f(130, 50));
        btnLevels[i].setFillColor(sf::Color(46, 204, 113));
        int row = i / 2;
        int col = i % 2;
        float x = (col == 0) ? (WINDOW_WIDTH / 2 - 145) : (WINDOW_WIDTH / 2 + 15);
        float y = 200 + row * 70;
        btnLevels[i].setPosition(x, y);
    }

    // Nút "Quay lại" dùng chung cho các menu phụ
    sf::RectangleShape btnBackCommon(sf::Vector2f(150, 40));
    btnBackCommon.setPosition(WINDOW_WIDTH / 2 - 75, 530);
    btnBackCommon.setFillColor(sf::Color(127, 140, 141));

    // Nút "Quay lại Menu" dùng trong trận
    sf::RectangleShape btnBackInGame(sf::Vector2f(150, 35));
    btnBackInGame.setPosition(WINDOW_WIDTH - 170, BOARD_SIZE + GRID_OFFSET + 20);
    btnBackInGame.setFillColor(sf::Color(127, 140, 141));

    // Nút "Xem đáp án" và "Quay lại Menu" khi thua cuộc (GAME_OVER) - chưa xem đáp án
    sf::RectangleShape btnViewAnswer(sf::Vector2f(180, 45));
    btnViewAnswer.setPosition(WINDOW_WIDTH/2 - 190, 330); btnViewAnswer.setFillColor(sf::Color(230, 126, 34));
    sf::RectangleShape btnBackFromGameOver(sf::Vector2f(180, 45));
    btnBackFromGameOver.setPosition(WINDOW_WIDTH/2 + 10, 330); btnBackFromGameOver.setFillColor(sf::Color(127, 140, 141));

    // Nút "Quay lại Menu" cố định khi ĐÃ xem đáp án (đặt dưới cùng màn hình, không đè bảng giải)
    sf::RectangleShape btnBackAfterReveal(sf::Vector2f(220, 40));
    btnBackAfterReveal.setPosition(WINDOW_WIDTH/2 - 110, WINDOW_HEIGHT - 50);
    btnBackAfterReveal.setFillColor(sf::Color(127, 140, 141));

    // ═══════════════════════════════════════════
    //  VÒNG LẶP XỬ LÝ SỰ KIỆN (GAME LOOP)
    // ═══════════════════════════════════════════
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();

            sf::Vector2i mousePos = sf::Mouse::getPosition(window);
            sf::Vector2f mouseCoords = window.mapPixelToCoords(mousePos);

            // 1. MAIN MENU
            if (mode == GameMode::MAIN_MENU) {
                if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                    if (btnPractice.getGlobalBounds().contains(mouseCoords.x, mouseCoords.y)) {
                        auto full = generator.generateFull();
                        auto puzzle = generator.generateMaxPuzzle(full);
                        board.reset(); board.puzzle = puzzle; board.solution = full;
                        selectedRow = selectedCol = -1; wrongMoves = 0;
                        gameClock.restart(); mode = GameMode::PLAYING_PRACTICE;
                    }
                    else if (btnChallengeMenu.getGlobalBounds().contains(mouseCoords.x, mouseCoords.y)) {
                        mode = GameMode::CHALLENGE_TYPE_SELECT; // Chọn Loại 1 / Loại 2 trước
                    }
                    else if (btnHistory.getGlobalBounds().contains(mouseCoords.x, mouseCoords.y)) {
                        mode = GameMode::HISTORY_MENU;
                    }
                }
            }
            // 2. MÀN HÌNH CHỌN LOẠI THỬ THÁCH (CHALLENGE_TYPE_SELECT)
            else if (mode == GameMode::CHALLENGE_TYPE_SELECT) {
                if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                    if (btnBackCommon.getGlobalBounds().contains(mouseCoords.x, mouseCoords.y)) {
                        mode = GameMode::MAIN_MENU;
                    } else if (btnType1.getGlobalBounds().contains(mouseCoords.x, mouseCoords.y)) {
                        challengeType = 1;
                        mode = GameMode::CHALLENGE_SELECT;
                    } else if (btnType2.getGlobalBounds().contains(mouseCoords.x, mouseCoords.y)) {
                        challengeType = 2;
                        mode = GameMode::CHALLENGE_SELECT;
                    }
                }
                if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
                    mode = GameMode::MAIN_MENU;
                }
            }
            // 3. MÀN HÌNH CHỌN LEVEL THỬ THÁCH (CHALLENGE_SELECT)
            else if (mode == GameMode::CHALLENGE_SELECT) {
                if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                    if (btnBackCommon.getGlobalBounds().contains(mouseCoords.x, mouseCoords.y)) {
                        mode = GameMode::CHALLENGE_TYPE_SELECT; // Lùi về chọn lại Loại
                    }
                    for (int i = 0; i < 8; i++) {
                        if (btnLevels[i].getGlobalBounds().contains(mouseCoords.x, mouseCoords.y)) {
                            challengeLevel = i + 1; // Nhận diện Level 1..8
                            
                            auto full = generator.generateFull();
                            auto puzzle = generator.generateMaxPuzzle(full);
                            board.reset(); board.puzzle = puzzle; board.solution = full;
                            selectedRow = selectedCol = -1; wrongMoves = 0;
                            isGameOverReasonTime = false;
                            revealAnswer = false;
                            
                            gameClock.restart(); 
                            mode = GameMode::PLAYING_CHALLENGE;
                            break;
                        }
                    }
                }
                if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
                    mode = GameMode::CHALLENGE_TYPE_SELECT;
                }
            }
            // 4. LỊCH SỬ & XẾP HẠNG
            else if (mode == GameMode::HISTORY_MENU) {
                if ((event.type == sf::Event::MouseButtonPressed && btnBackCommon.getGlobalBounds().contains(mouseCoords.x, mouseCoords.y)) || 
                    (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape)) {
                    mode = GameMode::MAIN_MENU;
                }
            }
            // 5. XỬ LÝ KHI ĐANG CHƠI
            else if (mode == GameMode::PLAYING_PRACTICE || mode == GameMode::PLAYING_CHALLENGE) {
                if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                    if (btnBackInGame.getGlobalBounds().contains(mouseCoords.x, mouseCoords.y)) {
                        mode = GameMode::MAIN_MENU;
                    }
                    else if (mouseCoords.x >= GRID_OFFSET && mouseCoords.x < GRID_OFFSET + BOARD_SIZE &&
                             mouseCoords.y >= GRID_OFFSET && mouseCoords.y < GRID_OFFSET + BOARD_SIZE) {
                        selectedCol = (mouseCoords.x - GRID_OFFSET) / CELL_SIZE;
                        selectedRow = (mouseCoords.y - GRID_OFFSET) / CELL_SIZE;
                    }
                }

                if (event.type == sf::Event::TextEntered && selectedRow != -1 && selectedCol != -1) {
                    char enteredChar = static_cast<char>(event.text.unicode);
                    if (enteredChar >= '1' && enteredChar <= '9') {
                        if (!board.isFixed(selectedRow, selectedCol)) {
                            bool ok = board.set(selectedRow, selectedCol, enteredChar - '0');
                            // Loại 2 (PLAYING_CHALLENGE + challengeType==2): không đếm lỗi, không thua vì lỗi,
                            // chỉ thời gian quyết định thắng/thua.
                            if (!ok) {
                                if (mode == GameMode::PLAYING_CHALLENGE && challengeType == 1) {
                                    wrongMoves++;
                                    if (wrongMoves > maxWrongByLvl_T1[challengeLevel - 1]) {
                                        history.push_back({"Thu Thach", elapsedSeconds, wrongMoves, false, challengeLevel, challengeType});
                                        revealAnswer = false;
                                        mode = GameMode::GAME_OVER;
                                    }
                                } else if (mode == GameMode::PLAYING_PRACTICE) {
                                    wrongMoves++;
                                }
                                // challengeType == 2: không tăng wrongMoves, không kiểm tra thua ở đây
                            }
                        }
                    } else if (enteredChar == '0' || enteredChar == 8) {
                        board.set(selectedRow, selectedCol, 0);
                    }
                }

                if (board.complete()) {
                    if (mode == GameMode::PLAYING_PRACTICE) {
                        history.push_back({"Luyen Tap", elapsedSeconds, wrongMoves, true, 0, 0});
                    } else {
                        history.push_back({"Thu Thach", elapsedSeconds, wrongMoves, true, challengeLevel, challengeType});
                    }
                    mode = GameMode::VICTORY;
                }
            }
            // 6. MÀN HÌNH THUA CUỘC (GAME_OVER) - cho phép xem đáp án trước khi về Menu
            else if (mode == GameMode::GAME_OVER) {
                if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                    if (!revealAnswer) {
                        if (btnViewAnswer.getGlobalBounds().contains(mouseCoords.x, mouseCoords.y)) {
                            revealAnswer = true; // Chỉ hiển thị đáp án, KHÔNG rời màn hình
                        } else if (btnBackFromGameOver.getGlobalBounds().contains(mouseCoords.x, mouseCoords.y)) {
                            mode = GameMode::MAIN_MENU;
                        }
                    } else {
                        if (btnBackAfterReveal.getGlobalBounds().contains(mouseCoords.x, mouseCoords.y)) {
                            revealAnswer = false;
                            mode = GameMode::MAIN_MENU;
                        }
                    }
                }
                if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
                    revealAnswer = false;
                    mode = GameMode::MAIN_MENU;
                }
            }
            // 7. MÀN HÌNH CHIẾN THẮNG (VICTORY)
            else if (mode == GameMode::VICTORY) {
                if (event.type == sf::Event::MouseButtonPressed || event.type == sf::Event::KeyPressed) {
                    mode = GameMode::MAIN_MENU;
                }
            }
        }

        // Cập nhật bộ đếm thời gian
        if (mode == GameMode::PLAYING_PRACTICE) {
            elapsedSeconds = gameClock.getElapsedTime().asSeconds();
        } else if (mode == GameMode::PLAYING_CHALLENGE) {
            elapsedSeconds = gameClock.getElapsedTime().asSeconds();
            int maxTime = (challengeType == 1) ? maxTimeByLvl_T1[challengeLevel - 1] : maxTimeByLvl_T2[challengeLevel - 1];
            countdownSeconds = maxTime - elapsedSeconds;
            if (countdownSeconds <= 0) {
                isGameOverReasonTime = true;
                history.push_back({"Thu Thach", elapsedSeconds, wrongMoves, false, challengeLevel, challengeType});
                revealAnswer = false;
                mode = GameMode::GAME_OVER;
            }
        }

        // ═══════════════════════════════════════════
        //  XỬ LÝ VẼ GIAO DIỆN (RENDERING)
        // ═══════════════════════════════════════════
        window.clear(sf::Color(245, 242, 235));

        // --- SCREEN: MAIN MENU ---
        if (mode == GameMode::MAIN_MENU) {
            sf::Text title("SUDOKU LITE - HUST", font, 36);
            title.setStyle(sf::Text::Bold); title.setFillColor(sf::Color(44, 62, 80));
            title.setPosition(WINDOW_WIDTH/2 - title.getGlobalBounds().width/2, 120);
            window.draw(title);

            window.draw(btnPractice); window.draw(btnChallengeMenu); window.draw(btnHistory);

            auto drawMenuTxt = [&](const std::string& s, sf::RectangleShape& b) {
                sf::Text t(s, font, 18); t.setFillColor(sf::Color::White);
                t.setPosition(b.getPosition().x + b.getSize().x/2 - t.getGlobalBounds().width/2, b.getPosition().y + 14);
                window.draw(t);
            };
            drawMenuTxt("CHEDO LUYEN TAP", btnPractice);
            drawMenuTxt("CHEDO THU THACH", btnChallengeMenu);
            drawMenuTxt("BANG XEP HANG", btnHistory);
        }
        // --- SCREEN: CHỌN LOẠI THỬ THÁCH (CHALLENGE_TYPE_SELECT) ---
        else if (mode == GameMode::CHALLENGE_TYPE_SELECT) {
            sf::Text title("CHON LOAI THU THACH", font, 30);
            title.setStyle(sf::Text::Bold); title.setFillColor(sf::Color(230, 126, 34));
            title.setPosition(WINDOW_WIDTH/2 - title.getGlobalBounds().width/2, 130);
            window.draw(title);

            window.draw(btnType1);
            sf::Text t1("LOAI 1: DEM LOI SAI", font, 18); t1.setStyle(sf::Text::Bold); t1.setFillColor(sf::Color::White);
            t1.setPosition(btnType1.getPosition().x + 20, btnType1.getPosition().y + 12);
            sf::Text d1("Bao do dung/sai tung o. Sai qua so\nluot quy dinh theo cap do la THUA.", font, 13); d1.setFillColor(sf::Color(220, 230, 240));
            d1.setPosition(btnType1.getPosition().x + 20, btnType1.getPosition().y + 42);
            window.draw(t1); window.draw(d1);

            window.draw(btnType2);
            sf::Text t2("LOAI 2: GIOI HAN THOI GIAN", font, 18); t2.setStyle(sf::Text::Bold); t2.setFillColor(sf::Color::White);
            t2.setPosition(btnType2.getPosition().x + 20, btnType2.getPosition().y + 12);
            sf::Text d2("Khong bao dung/sai. Het gio ma chua\nxong bang la THUA, du dien sai bao nhieu.", font, 13); d2.setFillColor(sf::Color(250, 220, 215));
            d2.setPosition(btnType2.getPosition().x + 20, btnType2.getPosition().y + 42);
            window.draw(t2); window.draw(d2);

            window.draw(btnBackCommon);
            sf::Text tBack("QUAY LAI", font, 16); tBack.setFillColor(sf::Color::White);
            tBack.setPosition(btnBackCommon.getPosition().x + btnBackCommon.getSize().x/2 - tBack.getGlobalBounds().width/2, btnBackCommon.getPosition().y + 10);
            window.draw(tBack);
        }
        // --- SCREEN: CHỌN MỨC ĐỘ THỬ THÁCH (CHALLENGE_SELECT) ---
        else if (mode == GameMode::CHALLENGE_SELECT) {
            std::string typeLabel = (challengeType == 1) ? "LOAI 1: DEM LOI SAI" : "LOAI 2: GIOI HAN THOI GIAN";
            sf::Text title("CHON CAP DO - " + typeLabel, font, 22);
            title.setStyle(sf::Text::Bold); title.setFillColor(sf::Color(230, 126, 34));
            title.setPosition(WINDOW_WIDTH/2 - title.getGlobalBounds().width/2, 110);
            window.draw(title);

            for (int i = 0; i < 8; i++) {
                window.draw(btnLevels[i]);
                std::string sub = (challengeType == 1)
                    ? (formatTime(maxTimeByLvl_T1[i]) + " | Loi<=" + std::to_string(maxWrongByLvl_T1[i]))
                    : (formatTime(maxTimeByLvl_T2[i]) + " | Khong bao loi");
                sf::Text t("LV " + std::to_string(i + 1), font, 16);
                t.setFillColor(sf::Color::White); t.setStyle(sf::Text::Bold);
                t.setPosition(btnLevels[i].getPosition().x + 10, btnLevels[i].getPosition().y + 6);
                sf::Text tSub(sub, font, 11); tSub.setFillColor(sf::Color(235, 245, 235));
                tSub.setPosition(btnLevels[i].getPosition().x + 10, btnLevels[i].getPosition().y + 28);
                window.draw(t); window.draw(tSub);
            }

            window.draw(btnBackCommon);
            sf::Text tBack("QUAY LAI", font, 16); tBack.setFillColor(sf::Color::White);
            tBack.setPosition(btnBackCommon.getPosition().x + btnBackCommon.getSize().x/2 - tBack.getGlobalBounds().width/2, btnBackCommon.getPosition().y + 10);
            window.draw(tBack);
        }
        // --- SCREEN: BẢNG XẾP HẠNG ---
        else if (mode == GameMode::HISTORY_MENU) {
            sf::Text title("BANG XEP HANG CHI TIET", font, 28);
            title.setFillColor(sf::Color(44, 62, 80)); title.setPosition(WINDOW_WIDTH/2 - title.getGlobalBounds().width/2, 40);
            window.draw(title);

            std::vector<HistoryEntry> practiceRank;
            std::vector<HistoryEntry> challengeHist;
            for (auto& h : history) {
                if (h.modeName == "Luyen Tap" && h.isWin) practiceRank.push_back(h);
                else if (h.modeName == "Thu Thach") challengeHist.push_back(h);
            }

            std::sort(practiceRank.begin(), practiceRank.end(), [](const HistoryEntry& a, const HistoryEntry& b){
                if (a.wrongMoves != b.wrongMoves) return a.wrongMoves < b.wrongMoves;
                return a.duration < b.duration;
            });

            sf::Text tHead1("TOP 5 TRAN LUYEN TAP (IT LOI NHAT)", font, 16); tHead1.setFillColor(sf::Color(52, 152, 219)); tHead1.setPosition(40, 110); window.draw(tHead1);
            for(size_t i=0; i<5 && i<practiceRank.size(); i++) {
                std::string row = "Top " + std::to_string(i+1) + " -> Loi: " + std::to_string(practiceRank[i].wrongMoves) + " | Thoi gian: " + formatTime(practiceRank[i].duration);
                sf::Text tRow(row, font, 14); tRow.setFillColor(sf::Color::Black); tRow.setPosition(40, 140 + i*25); window.draw(tRow);
            }

            sf::Text tHead2("CAC TRAN THU THACH GAN NHAT", font, 16); tHead2.setFillColor(sf::Color(230, 126, 34)); tHead2.setPosition(40, 320); window.draw(tHead2);
            std::reverse(challengeHist.begin(), challengeHist.end());
            for(size_t i=0; i<5 && i<challengeHist.size(); i++) {
                std::string status = challengeHist[i].isWin ? "[THANG]" : "[THUA]";
                std::string typeTag = (challengeHist[i].challengeType == 1) ? "L1" : "L2";
                std::string row = "LV " + std::to_string(challengeHist[i].level) + " (" + typeTag + ") | " + status + " | Tg: " + formatTime(challengeHist[i].duration) + " | Loi: " + std::to_string(challengeHist[i].wrongMoves);
                sf::Text tRow(row, font, 14); 
                tRow.setFillColor(challengeHist[i].isWin ? sf::Color(46, 204, 113) : sf::Color(231, 76, 60));
                tRow.setPosition(40, 350 + i*25); window.draw(tRow);
            }

            window.draw(btnBackCommon);
            sf::Text tBack("QUAY LAI", font, 16); tBack.setFillColor(sf::Color::White);
            tBack.setPosition(btnBackCommon.getPosition().x + btnBackCommon.getSize().x/2 - tBack.getGlobalBounds().width/2, btnBackCommon.getPosition().y + 10);
            window.draw(tBack);
        }
        // --- SCREEN: TRẬN ĐẤU (Luyện tập & Thử thách) ---
        else if (mode == GameMode::PLAYING_PRACTICE || mode == GameMode::PLAYING_CHALLENGE) {
            
            if (selectedRow != -1 && selectedCol != -1) {
                sf::RectangleShape hl(sf::Vector2f(CELL_SIZE, CELL_SIZE));
                hl.setPosition(GRID_OFFSET + selectedCol * CELL_SIZE, GRID_OFFSET + selectedRow * CELL_SIZE);
                hl.setFillColor(sf::Color(52, 152, 219, 60)); window.draw(hl);
            }

            bool hideErrorFeedback = (mode == GameMode::PLAYING_CHALLENGE && challengeType == 2);

            for (int r = 0; r < 9; r++) {
                for (int c = 0; c < 9; c++) {
                    int val = board.get(r, c);
                    if (val != 0) {
                        sf::Text text(std::to_string(val), font, 30);
                        if (board.isFixed(r, c)) {
                            text.setFillColor(sf::Color(44, 62, 80));
                        } else if (!hideErrorFeedback && board.err[r][c]) {
                            text.setFillColor(sf::Color(231, 76, 60)); // chỉ báo đỏ khi KHÔNG phải Loại 2
                        } else {
                            text.setFillColor(sf::Color(41, 128, 185)); // Loại 2: luôn hiển thị màu trung tính, không lộ đúng/sai
                        }

                        sf::FloatRect textRect = text.getLocalBounds();
                        text.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
                        text.setPosition(GRID_OFFSET + c * CELL_SIZE + CELL_SIZE / 2.0f, GRID_OFFSET + r * CELL_SIZE + CELL_SIZE / 2.0f);
                        window.draw(text);
                    }
                }
            }

            for (int i = 0; i <= 9; i++) {
                sf::RectangleShape vLine, hLine;
                if (i % 3 == 0) {
                    vLine.setSize(sf::Vector2f(4, BOARD_SIZE)); vLine.setFillColor(sf::Color(44, 62, 80));
                    hLine.setSize(sf::Vector2f(BOARD_SIZE, 4)); hLine.setFillColor(sf::Color(44, 62, 80));
                    vLine.setPosition(GRID_OFFSET + i * CELL_SIZE - 2, GRID_OFFSET);
                    hLine.setPosition(GRID_OFFSET, GRID_OFFSET + i * CELL_SIZE - 2);
                } else {
                    vLine.setSize(sf::Vector2f(1, BOARD_SIZE)); vLine.setFillColor(sf::Color(189, 195, 199));
                    hLine.setSize(sf::Vector2f(BOARD_SIZE, 1)); hLine.setFillColor(sf::Color(189, 195, 199));
                    vLine.setPosition(GRID_OFFSET + i * CELL_SIZE, GRID_OFFSET);
                    hLine.setPosition(GRID_OFFSET, GRID_OFFSET + i * CELL_SIZE);
                }
                window.draw(vLine); window.draw(hLine);
            }

            std::string statsStr;
            if (mode == GameMode::PLAYING_PRACTICE) {
                statsStr = "LUYEN TAP | T.gian: " + formatTime(elapsedSeconds) + " | So loi: " + std::to_string(wrongMoves);
            } else if (challengeType == 1) {
                int maxW = maxWrongByLvl_T1[challengeLevel - 1];
                statsStr = "THU THACH LV " + std::to_string(challengeLevel) + " (L1) | Con lai: " + formatTime(countdownSeconds) + " | Loi: " + std::to_string(wrongMoves) + "/" + std::to_string(maxW);
            } else {
                statsStr = "THU THACH LV " + std::to_string(challengeLevel) + " (L2) | Con lai: " + formatTime(countdownSeconds) + " | Khong bao loi";
            }
            sf::Text txtStats(statsStr, font, 14); txtStats.setFillColor(sf::Color(44, 62, 80));
            txtStats.setPosition(GRID_OFFSET, BOARD_SIZE + GRID_OFFSET + 20); window.draw(txtStats);

            window.draw(btnBackInGame);
            sf::Text txtBackInGame("<< MENU", font, 14); txtBackInGame.setFillColor(sf::Color::White);
            txtBackInGame.setPosition(btnBackInGame.getPosition().x + 40, btnBackInGame.getPosition().y + 8);
            window.draw(txtBackInGame);

            sf::Text txtGuide("Phim: 1-9 (Dien) | 0/Backspace (Xoa)", font, 14);
            txtGuide.setFillColor(sf::Color(127, 140, 141)); txtGuide.setPosition(GRID_OFFSET, BOARD_SIZE + GRID_OFFSET + 55); window.draw(txtGuide);
        }
        // --- SCREEN: THUA CUỘC (GAME_OVER) - co the bam xem dap an truoc khi ve Menu ---
        else if (mode == GameMode::GAME_OVER) {
            sf::Text t1("BAN DA THUA CUOC!", font, 34); t1.setFillColor(sf::Color(231, 76, 60)); t1.setStyle(sf::Text::Bold);
            t1.setPosition(WINDOW_WIDTH/2 - t1.getGlobalBounds().width/2, 80); window.draw(t1);

            std::string reason = isGameOverReasonTime ? "Ly do: Het thoi gian quy dinh!" : "Ly do: Vuot qua so luot loi cho phep!";
            sf::Text t2(reason, font, 20); t2.setFillColor(sf::Color(44, 62, 80)); t2.setPosition(WINDOW_WIDTH/2 - t2.getGlobalBounds().width/2, 140); window.draw(t2);

            if (!revealAnswer) {
                // Chưa bấm xem đáp án: chỉ hiện 2 nút lựa chọn, KHÔNG lộ bảng giải
                window.draw(btnViewAnswer);
                sf::Text tv("XEM DAP AN", font, 16); tv.setStyle(sf::Text::Bold); tv.setFillColor(sf::Color::White);
                tv.setPosition(btnViewAnswer.getPosition().x + btnViewAnswer.getSize().x/2 - tv.getGlobalBounds().width/2, btnViewAnswer.getPosition().y + 14);
                window.draw(tv);

                window.draw(btnBackFromGameOver);
                sf::Text tb("VE MENU CHINH", font, 16); tb.setStyle(sf::Text::Bold); tb.setFillColor(sf::Color::White);
                tb.setPosition(btnBackFromGameOver.getPosition().x + btnBackFromGameOver.getSize().x/2 - tb.getGlobalBounds().width/2, btnBackFromGameOver.getPosition().y + 14);
                window.draw(tb);
            } else {
                // Đã bấm xem đáp án: vẽ bảng đáp án đầy đủ (thu nhỏ ô lại để vừa khung) + nút quay về Menu cố định dưới cùng
                const float ansCell = 50.0f;
                const float ansOffsetX = WINDOW_WIDTH/2 - (ansCell * 9) / 2.0f;
                const float ansOffsetY = 200.0f;
                for (int r = 0; r < 9; r++) {
                    for (int c = 0; c < 9; c++) {
                        int val = board.solution[r][c];
                        sf::Text text(std::to_string(val), font, 22);
                        text.setFillColor(board.isFixed(r, c) ? sf::Color(44, 62, 80) : sf::Color(230, 126, 34));
                        sf::FloatRect textRect = text.getLocalBounds();
                        text.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
                        text.setPosition(ansOffsetX + c * ansCell + ansCell / 2.0f, ansOffsetY + r * ansCell + ansCell / 2.0f);
                        window.draw(text);
                    }
                }
                // Lưới mỏng cho bảng đáp án thu nhỏ
                for (int i = 0; i <= 9; i++) {
                    sf::RectangleShape vL(sf::Vector2f(i % 3 == 0 ? 2.f : 1.f, ansCell * 9));
                    vL.setFillColor(i % 3 == 0 ? sf::Color(44,62,80) : sf::Color(200,200,200));
                    vL.setPosition(ansOffsetX + i * ansCell, ansOffsetY);
                    window.draw(vL);
                    sf::RectangleShape hL(sf::Vector2f(ansCell * 9, i % 3 == 0 ? 2.f : 1.f));
                    hL.setFillColor(i % 3 == 0 ? sf::Color(44,62,80) : sf::Color(200,200,200));
                    hL.setPosition(ansOffsetX, ansOffsetY + i * ansCell);
                    window.draw(hL);
                }

                sf::Text tHint("Mau cam la dap an dung. Bam nut duoi de ve Menu", font, 14);
                tHint.setFillColor(sf::Color(127, 140, 141));
                tHint.setPosition(WINDOW_WIDTH/2 - tHint.getGlobalBounds().width/2, ansOffsetY + ansCell * 9 + 15);
                window.draw(tHint);

                window.draw(btnBackAfterReveal);
                sf::Text tb2("VE MENU CHINH", font, 16); tb2.setStyle(sf::Text::Bold); tb2.setFillColor(sf::Color::White);
                tb2.setPosition(btnBackAfterReveal.getPosition().x + btnBackAfterReveal.getSize().x/2 - tb2.getGlobalBounds().width/2, btnBackAfterReveal.getPosition().y + 10);
                window.draw(tb2);
            }
        }
        // --- SCREEN: VICTORY ---
        else if (mode == GameMode::VICTORY) {
            sf::Text t1("CHIEN THANG!", font, 36); t1.setFillColor(sf::Color(46, 204, 113)); t1.setStyle(sf::Text::Bold);
            t1.setPosition(WINDOW_WIDTH/2 - t1.getGlobalBounds().width/2, 200); window.draw(t1);

            sf::Text t2("Thoi gian: " + formatTime(elapsedSeconds) + " | Tong loi sai: " + std::to_string(wrongMoves), font, 20);
            t2.setFillColor(sf::Color(44, 62, 80)); t2.setPosition(WINDOW_WIDTH/2 - t2.getGlobalBounds().width/2, 280); window.draw(t2);

            sf::Text t3("Bam nut bat ky de ve Menu", font, 16); t3.setFillColor(sf::Color(149, 165, 166)); t3.setPosition(WINDOW_WIDTH/2 - t3.getGlobalBounds().width/2, 450); window.draw(t3);
        }

        window.display();
    }

    return 0;
}
