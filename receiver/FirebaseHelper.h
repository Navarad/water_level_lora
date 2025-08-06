#ifndef FIREBASE_HELPER_H
#define FIREBASE_HELPER_H
#define ENABLE_FIRESTORE
#define FIREBASE_PROJECT_ID "waterlevelmonitor-90dd4"

#include <FirebaseClient.h>
#include <time.h> // nezabudni na začiatku

void processData(AsyncResult &aResult);
String getTimestampString(uint64_t sec, uint32_t nano);
Document<Values::Value> createWaterLevelDocument(double hladina, double baterka);
void create_document_async(Firestore::Documents &Docs, AsyncClientClass &aClient, Document<Values::Value> &doc, const String &documentPath);

#endif
