#include "myMosqConcrete.h"

myMosqConcrete::myMosqConcrete(const char* id, const char* _topic, const char* host, int port)
        : myMosq(id, _topic, host, port), publishedNodes(0) {
    std::cout << "Master node mqtt setup" << std::endl;
}

/*
struct mosquitto_message{
        int mid;
        char *topic;
        void *payload;
        int payloadlen;
        int qos;
        bool retain;
};
*/
bool myMosqConcrete::receive_message(const struct mosquitto_message* message) {
    std::cout << "Master node is processing received forests..." << std::endl;
    std::string topic(message->topic);
    std::cout << "Message from broker with topic: " << topic << std::endl;
    if (topic.find("node") == std::string::npos) {
        return false;
    } 

    char* pchar = (char*)(message->payload);
    std::string str(pchar);
    //Should also check the values of the message...
    std::cout << "Message from broker: " << str << std::endl;

    //fix next time for now hardcoded
    if (topic.find("node0") != std::string::npos) {
        checkNodePayload(0, str, topic);
    } else if (topic.find("node1") != std::string::npos) {
        checkNodePayload(1, str, topic);
    } else if (topic.find("node2") != std::string::npos) {
        checkNodePayload(2, str, topic);
    }


    if (publishedNodes.size() == 3) {
        distributedTest();
    }

    return true;
}

void myMosqConcrete::checkNodePayload(int n, std::string str, std::string topic) {
    std::stringstream ss;
    ss << "Receiving node " << n << " message...";
    std::cout << ss.str() << std::endl;

    if (std::find(publishedNodes.begin(), publishedNodes.end(), n) == publishedNodes.end()) {
        publishedNodes.push_back(n);
        std::stringstream rts;
        rts << "RTs_Forest_" << n << ".txt";
        writeToFile(str.c_str(), rts.str());

        //must fix something in slave because right now the ACK triggers their processing
        //maybe different topic or parse there
        // std::string msg("ACK");
        // std::stringstream s2;
        // s2 << "slave/node" << n;
        // this->send_message(s2.str().c_str(), msg.c_str());

    }
}

void myMosqConcrete::distributedTest() {
    Utils::Json *json = new Utils::Json();
    Utils::Configs c = json->parseJsonFile("configs.json");

    std::vector<std::string> nodeList = c.nodeList;
    
    //Have to loop through all of the node list
    std::vector<std::vector<int>> scoreVectors;
    std::vector<int> correctLabel;
    std::vector<RTs::Forest> randomForests;
    //Assume to read RTs_Forest.txt
    char dir[255];
    getcwd(dir,255);
    std::cout << dir << std::endl;


    for (unsigned int i = 0; i < nodeList.size(); ++i) {
        RTs::Forest rts_forest;
        std::stringstream ss;
        ss << dir << "/RTs_Forest_" << i << ".txt";//double check
        rts_forest.Load(ss.str());
        randomForests.push_back(rts_forest);
        ss.str(std::string());
    }
    //todo: process the rts_forest, load fxn already created the node
    // read the csv file here
    Utils::Parser *p = new Utils::Parser();
    p->setClassColumn(1);
    std::vector<RTs::Sample> samples = p->readCSVToSamples("cleaned.csv");

    //Too many loops for testing
    //Need to change checkScores func to just accept the samples vector (too large? const)
    std::for_each(samples.begin(), samples.end(), [&](const RTs::Sample& s) {
        correctLabel.push_back(s.label);
    });

    for (unsigned int i = 0; i < samples.size(); ++i) {
        std::vector<int> nodeListSamples(0, 3);
        scoreVectors.push_back(nodeListSamples);
        for (unsigned int j = 0; j < nodeList.size(); ++j) {
            RTs::Feature f = samples[i].feature_vec;
            const float* histo = randomForests[j].EstimateClass(f);
            scoreVectors[i].push_back(getClassNumberFromHistogram(10, histo));
        }
    }

    Utils::TallyScores *ts = new Utils::TallyScores();
    ts->checkScores(correctLabel, scoreVectors);
    delete ts;
}

int myMosqConcrete::getClassNumberFromHistogram(int numberOfClasses, const float* histogram) {
    float biggest = -FLT_MAX;
    int index = -1;

    //since classes start at 1
    for (int i = 0; i < numberOfClasses; ++i) {
        float f = *(histogram + i);
        if (f > biggest) {
            index = i;
            biggest = f;
        }
    }
    return index;
}
