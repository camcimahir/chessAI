#include "Application.h"
#include "imgui/imgui.h"
#include "classes/TicTacToe.h"
#include "classes/Checkers.h"
#include "classes/Othello.h"
#include "classes/Chess.h"

namespace ClassGame {
        //
        // our global variables
        //
        Game *game = nullptr;
        bool gameOver = false;
        int gameWinner = -1;

        //
        // game starting point
        // this is called by the main render loop in main.cpp
        //
        void GameStartUp() 
        {
            game = nullptr;
        }

        //
        // game render loop
        // this is called by the main render loop in main.cpp
        //
        void RenderGame() 
        {
                ImGui::DockSpaceOverViewport();

                //ImGui::ShowDemoWindow();

                ImGui::Begin("Settings");

                if (gameOver) {
                    ImGui::Text("Game Over!");
                    if (gameWinner == -1) {
                        ImGui::Text("Result: Draw");
                    } else {
                        const char* winnerName = (gameWinner == 0) ? "White Player" : "Black Player";
                        ImGui::Text("Winner: %s", winnerName);
                    }
                    if (ImGui::Button("Reset Game")) {
                        game->stopGame();
                        game->setUpBoard();
                        gameOver = false;
                        gameWinner = -1;
                    }
                }
                static bool chessSetupMode = false;

                if (!game) {
                    if (ImGui::Button("Start Tic-Tac-Toe")) {
                        game = new TicTacToe();
                        game->setUpBoard();
                    }
                    if (ImGui::Button("Start Checkers")) {
                        game = new Checkers();
                        game->setUpBoard();
                    }
                    if (ImGui::Button("Start Othello")) {
                        game = new Othello();
                        game->setUpBoard();
                    }

                    if (!chessSetupMode) {
                        if (ImGui::Button("Start Chess")) {
                            chessSetupMode = true;
                        }
                    } else {
                        ImGui::Text("Select Chess Game Mode:");

                        // Human vs Human
                        if (ImGui::Button("Chess: Human vs Human")) {
                            Chess* chess = new Chess();
                            chess->setUseAI(false);        // no AI; both sides human
                            game = chess;
                            game->setUpBoard();
                            chessSetupMode = false;
                        }

                        // Human vs AI (player as White, AI as Black)
                        if (ImGui::Button("Chess: Human vs AI (you as White)")) {
                            Chess* chess = new Chess();
                            chess->setUseAI(true);         // enable AI (Black)
                            game = chess;
                            game->setUpBoard();
                            chessSetupMode = false;
                        }

                        if (ImGui::Button("Cancel Chess Setup")) {
                            chessSetupMode = false;
                        }
                    }
                } else {
                    ImGui::Text("Current Player Number: %d", game->getCurrentPlayer()->playerNumber());
                    std::string stateString = game->stateString();
                    int stride = game->_gameOptions.rowX;
                    int height = game->_gameOptions.rowY;

                    for(int y=0; y<height; y++) {
                        ImGui::Text("%s", stateString.substr(y*stride,stride).c_str());
                    }
                    ImGui::Text("Current Board State: %s", game->stateString().c_str());
                }
                ImGui::End();

                ImGui::Begin("GameWindow");
                if (game) {
                    if (!gameOver && game->gameHasAI() && (game->getCurrentPlayer()->isAIPlayer() || game->_gameOptions.AIvsAI))
                    {
                        game->updateAI();
                    }
                    game->drawFrame();
                }
                ImGui::End();
        }

        //
        // end turn is called by the game code at the end of each turn
        // this is where we check for a winner
        //
        void EndOfTurn() 
        {
            Player *winner = game->checkForWinner();
            if (winner)
            {
                gameOver = true;
                gameWinner = winner->playerNumber();
            }
            if (game->checkForDraw()) {
                gameOver = true;
                gameWinner = -1;
            }
        }
}
