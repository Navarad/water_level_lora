#include "FirebaseHelper.h"

void processData(AsyncResult &aResult)
{
    if (!aResult.isResult()) return;

    if (aResult.isEvent()) {
        Serial.printf("Event: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.eventLog().message().c_str(), aResult.eventLog().code());
    }

    if (aResult.isDebug()) {
        Serial.printf("Debug: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
    }

    if (aResult.isError()) {
        Serial.printf("Error: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());
    }

    if (aResult.available()) {
        Serial.printf("Response: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());
    }
}

String getTimestampString(uint64_t sec, uint32_t nano)
{
    if (sec > 0x3afff4417f)
        sec = 0x3afff4417f;

    if (nano > 0x3b9ac9ff)
        nano = 0x3b9ac9ff;

    time_t now;
    struct tm ts;
    char buf[80];
    now = sec;
    ts = *localtime(&now);

    String format = "%Y-%m-%dT%H:%M:%S";

    if (nano > 0)
    {
        String fraction = String(double(nano) / 1000000000.0f, 9);
        fraction.remove(0, 1);
        format += fraction;
    }
    format += "Z";

    strftime(buf, sizeof(buf), format.c_str(), &ts);
    return buf;
}

// Funkcia vytvorí dokument s hodnotami
Document<Values::Value> createWaterLevelDocument(double hladina, double baterka) {
    time_t currentTime = time(nullptr);
    String tsString = getTimestampString(currentTime, 999999999);
    Values::TimestampValue tsV =  Values::TimestampValue(tsString);

    Document<Values::Value> doc;
    doc.add("depth", Values::Value(Values::DoubleValue(hladina)));
    doc.add("battery", Values::Value(Values::DoubleValue(baterka)));
    doc.add("timestamp", Values::Value(tsV));
    return doc;
}

void create_document_async(Firestore::Documents &Docs, AsyncClientClass &aClient, Document<Values::Value> &doc, const String &documentPath)
{
    Serial.println("Creating a document... ");

    // Async call with callback function.
    Docs.createDocument(aClient, Firestore::Parent(FIREBASE_PROJECT_ID), documentPath, DocumentMask(), doc, processData, "createDocumentTask");
}