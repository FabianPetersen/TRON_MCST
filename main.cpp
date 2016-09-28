#include <iostream>
#include <vector>
#include <unordered_map>
#include <random>
#include <unordered_set>
#include <math.h>
#include <chrono>
#include <string>

using namespace std;

// Maximum allowed duration
int maxDuration = 98;
int heuristicConstant = 1;
double biasParameter = 1 / sqrt(2);

random_device rseed;
mt19937 rgen(rseed()); // mersenne_twister


int randomIntRange(int min, int max){
    uniform_int_distribution<int> idist(min, max);
    return idist(rgen);
}


/**
 * Using a domain specific heuristic that should help the algorithm choose a better path
 * This will primarily be used for simulating moves for the evemies so that they dont loose as easily in the simulations
 * @param enemies
 * @param player
 * @return
 */
/*
double domainHeuristic(vector<Player>& enemies, Player& player){
    double heuristic = 0;

    for(Player e: enemies){
        heuristic += abs(e.position->getX() - player.position->getX()) + abs(e.position->getY() - player.position->getY());
    }

    return heuristic;
}
 */

class Point {
    int x, y;

    public:
    Point(int _x, int _y){
        x = _x;
        y = _y;
    }

    Point(){
        x = 0;
        y = 0;
    }

    const int getX() const {
        return x;
    }

    const int getY() const {
        return y;
    }

    bool operator==(const Point &other) const {
        return (x == other.x && y == other.y);
    }
};


namespace std
{
    template <>
    struct hash<Point>
    {
        size_t operator()(const Point& k) const
        {
            // Compute individual hash values for two data members and combine them using XOR and bit shifting
            return ((hash<int>()(k.getX()) ^ (hash<int>()(k.getY()) << 1)) >> 1);
        }
    };
}


vector<vector<Point*>> points;
int width = 30, height = 20;

class Board{
    unordered_set<Point*> blocked;

    public:
    Board(){}

    Board(unordered_set<Point*> _blocked){
        blocked = unordered_set<Point*>(_blocked);
    }

    void setBlocked(Point* p){
        blocked.insert(p);
    }

    bool isBlocked(Point* p){
        return blocked.find(p) != blocked.end();
    }

    Point* getPoint(int x, int y){
        if(x >= 0 && width > x && y >= 0 && height > y){
            return points.at(x).at(y);
        }else{
            return nullptr;
        }
    }

    vector<Point*> getNeighbors(int x, int y){
        vector<Point*> neighbors;

        Point* bottom = getPoint(x, y-1);
        Point* top = getPoint(x, y+1);
        Point* left = getPoint(x-1, y);
        Point* right = getPoint(x+1, y);

        if(top != nullptr){
            neighbors.push_back(top);
        }

        if(bottom != nullptr){
            neighbors.push_back(bottom);
        }

        if(left != nullptr){
            neighbors.push_back(left);
        }

        if(right != nullptr){
            neighbors.push_back(right);
        }

        return neighbors;
    }

    vector<Point*> validNeighbors(int x, int y){
        vector<Point*> valid;
        for(Point* p : getNeighbors(x, y)){
            if( !isBlocked(p) ){
                valid.push_back(p);
            }
        }

        return valid;
    }

};


class Player{
    public:
    Point* position;
    Player(Point* pos){
        position = pos;
    }

    Player(){}

    Point* getRandomMove(Board& board){
        vector<Point*> possibleMoves = board.validNeighbors(position->getX(), position->getY());

        if( !possibleMoves.empty()  ){
            int randomPos = (unsigned int) randomIntRange(1, possibleMoves.size()) - 1;
            return possibleMoves.at(randomPos);
        }else{
            // If the enemy cant return a valid choice it has lost
            return nullptr;
        }
    }


    Point* getSemiRandomMove(Board& board){
        vector<Point*> possibleMoves = board.validNeighbors(position->getX(), position->getY());

        if( !possibleMoves.empty()  ){

            int randomPos = (unsigned int) randomIntRange(1, possibleMoves.size()) - 1;
            return possibleMoves.at(randomPos);
        }else{
            // If the enemy cant return a valid choice it has lost
            return nullptr;
        }
    }
};



class ActionTree {
    public:
    Point* move;
    vector<ActionTree*> children;
    ActionTree* parent = nullptr;
    float wins = 0, plays = 0;
    bool winningMove;

    ActionTree(Point* _move, ActionTree* _parent){
        move = _move;
        wins = 0;
        plays = 0;
        winningMove = false;
        parent = _parent;
    }

    void win(){
        wins += 1;
        plays += 1;

        if( parent != nullptr){
            parent->win();
        }
    }

    void loss(){
        plays += 1;

        if( parent != nullptr ){
            parent->loss();
        }
    }

    double getVariance(){
        int totalWins = 0;
        int total = 0;
        int rewardSquared = 0;

        if( parent != nullptr){
            for(ActionTree* child : parent->children){
                total += child->plays;
                totalWins += child->wins;

                rewardSquared += pow(child->wins/child->plays, 2);
            }
        }

        if( total > 0 ){
            return rewardSquared - pow(totalWins/total, 2);
        }else {
            return 0;
        }
    }

    double ubc1TunedScore(){
        double winPercentage = 0, exploration = 0;
        // Avoid division by zero

        if (plays != 0){
            winPercentage = wins / plays;

            if( parent != nullptr ){
                // exploration = 2 * biasParameter * sqrt( 2*log(parent->plays) / plays );

                exploration = sqrt(  (log(parent->plays) / plays) *
                                     min(0.25,
                                     (getVariance() + sqrt(2 * log(parent->plays) / plays)  )));
            }
        }

        // +        return avg_payoff + math.sqrt(math.log(number_sampled) / sampled_arm.total) * min(MAX_BERNOULLI_RANDOM_VARIABLE_VARIANCE, variance + math.sqrt(2.0 * math.log(number_sampled) / sampled_arm.total))

        return winPercentage + exploration;
    }


    /* Get the best child based ubc score: higher is better */
    ActionTree* getOptimalChild(Board& board){
        ActionTree* bestChild = nullptr;
        double bestScore = -INFINITY;

        for(ActionTree* child : children){
            if( !board.isBlocked(child->move) ){
                double ubcScore = child->ubc1TunedScore();
                if( bestScore < ubcScore){
                    bestScore = ubcScore;
                    bestChild = child;
                }
            }
        }

        return bestChild;
    }

    ActionTree* getFinalChoiceChild(){
        ActionTree* bestChild = nullptr;
        double bestScore = -INFINITY;

        for(ActionTree* child : children){
            double score = child->plays;
            if( bestScore < score){
                bestScore = score;
                bestChild = child;
            }
        }

        return bestChild;
    }
};

class MCTS {
    ActionTree* rootNode;

    bool simulateMove(Point* move, Board board, vector<Player> enemies, Player player){
        if( board.isBlocked(move) ){
            return false;
        }else{
            board.setBlocked(move);
            player.position = move;
            return playSimulation(board, enemies, player);
        }
    }

    bool playSimulation(Board& board, vector<Player> enemies, Player player){
        // Run until we are the only one standing
        while( !enemies.empty() ){
            // The enemies makes the first move
            for(Player enemy : enemies){
                // Make a random move
                Point* move = enemy.getRandomMove(board);

                // Check if the move is a valid one i.e. not nullpointer
                if ( move == nullptr ){
                    // Needs to be adjused for more players
                    return true;
                }

                // Move the player to the new positon
                enemy.position = move;

                // And mark the old one as used
                board.setBlocked(move);
            }

            Point* playerMove = player.getRandomMove(board);
            if( playerMove == nullptr ){
                return false;
            }
            player.position = playerMove;
            board.setBlocked(playerMove);
        }
    }

    public:
    MCTS(){
        rootNode = nullptr;
    };

    Point* getBestChoice(Board board, vector<Player> enemies, Player player){
        if( rootNode == nullptr){
            rootNode = new ActionTree(player.position, nullptr);
        }

        auto begin = std::chrono::steady_clock::now();
        RESETLOOP:while ( int(chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count()) < maxDuration ) {
        //int count = 0;
        //RESETLOOP:while ( count++ < 150000) {
            // Start from the rootnode
            ActionTree* currentNode = rootNode;
            Board currentBoard = Board(board);
            vector<Player> currentEnemies;
            Player currentPlayer;

            while( true ){
                for(Player e: currentEnemies){
                    Point* newPosition = e.getRandomMove(board);
                    e.position = newPosition;

                    // They have lost
                    if( newPosition == nullptr){
                        currentNode->win();
                        goto RESETLOOP;
                    }else{
                        currentBoard.setBlocked(newPosition);
                    }
                }

                // The node has not been visited before, get neighbors as children
                if( currentNode->plays < 2 ){
                    vector<ActionTree*> children = {};
                    // Loop through and add children
                    for(Point* p: currentBoard.getNeighbors(currentNode->move->getX(), currentNode->move->getY())){
                        ActionTree* childAction = new ActionTree(p, currentNode);

                        // check if this is a valid move
                        if( simulateMove(p, currentBoard, enemies, player) ){
                            childAction->win();
                        }else {
                            childAction->loss();
                        }

                        // We need to add a simulation for each child
                        children.push_back(childAction);
                    }

                    // Add the children to the node
                    currentNode->children = children;

                    // Start again from rootnode
                    goto RESETLOOP;

                    // The node has been visited before
                }else{
                    ActionTree* bestChild = currentNode->getOptimalChild(currentBoard);
                    // if the best child is nullptr, we have lost
                    if( bestChild == nullptr ){
                        currentNode->loss();
                        goto RESETLOOP;
                    }else{
                        currentNode = bestChild;
                        currentBoard.setBlocked(currentNode->move);
                    }
                }
            }
        }

        // Continue using the branch that we choose -> more simulations will lead to
        // better result and this will lead to more and more simulations as the game progress.

        rootNode = rootNode->getFinalChoiceChild();
        // We cant do anything, do something just to end the turn
        if ( rootNode == nullptr){
            return new Point(-1, -1);
        }else{
            return rootNode->move;
        }

    };
};

string getDirection(Point* origin, Point* destination){
    if( origin->getX() == destination->getX() ){
        if( origin->getY() > destination->getY() ){
            return "UP";
        }else{
            return "DOWN";
        }
    }else{
        if( origin->getX() > destination->getX() ){
            return "LEFT";
        }else{
            return "RIGHT";
        }
    }
}


void gameLoop(Board& board, MCTS mcts){
    vector<Player> enemies;
    Player player;

    // THE FRIST POSITION IS NOT MARKED AS BLOCKED

    // game loop
    while (1) {
        int N; // total number of players (2 to 4).
        int P; // your player number (0 to 3).
        cin >> N >> P; cin.ignore();
        for (int i = 0; i < N; i++) {
            int X0; // starting X coordinate of lightcycle (or -1)
            int Y0; // starting Y coordinate of lightcycle (or -1)

            int X1; // starting X coordinate of lightcycle (can be the same as X0 if you play before this player)
            int Y1; // starting Y coordinate of lightcycle (can be the same as Y0 if you play before this player)

            cin >> X0 >> Y0 >> X1 >> Y1; cin.ignore();


            if( X0 != -1 && Y0 != -1 ){
                board.setBlocked(board.getPoint(X0, X1));
            }
            board.setBlocked(board.getPoint(X1, Y1));
            // This player is me
            if(i == P && enemies.size() != N){
                player = Player(board.getPoint(X1, Y1));
            }else if( i == P ){
                player.position = board.getPoint(X1, Y1);
            }else{
                if (enemies.size() != N){
                    enemies.push_back(Player(board.getPoint(X1, Y1)));
                }else {
                    enemies.at(i).position = board.getPoint(X1, Y1);
                }
            }
        }

        // Write an action using cout. DON'T FORGET THE "<< endl"
        // To debug: cerr << "Debug messages..." << endl;
        Point* result = mcts.getBestChoice(board, enemies, player);

        // IF no valid path exists
        if( result->getX() == -1 ){
            cout << "LEFT" << endl;
        }else{
            cout << getDirection(player.position, result) << endl;
        }
    }
}

void init(){
    for(int x = 0; x < width; x++){
        vector<Point*> yVector;
        for(int y = 0; y < height; y++){
            Point* p = new Point(x, y);
            yVector.push_back(p);
        }
        points.push_back(yVector);
    }
}


int main() {
    init();

    MCTS mcts = MCTS();
    Board board = Board();

    /*
    gameLoop(board, mcts);
    */



    Player enemy =  Player(board.getPoint(10, 10));
    vector<Player> enemies = { enemy };
    Player player = Player(board.getPoint(15, 15));

    board.setBlocked(player.position);
    board.setBlocked(enemy.position);

    Point* result = mcts.getBestChoice(board, enemies, player);
    cout << "The best move is: (" << result->getX() << ", " << result->getY() << ")" << std::endl;
    cout << "Direction: " << getDirection(player.position, result) << " FROM (" << player.position->getX() << ", " << player.position->getY() << ") TO (" << result->getX() << ", " << result->getY() << ")" << std::endl;

    return 0;
}