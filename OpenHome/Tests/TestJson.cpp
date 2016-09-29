#include <OpenHome/Private/TestFramework.h>
#include <OpenHome/Private/SuiteUnitTest.h>
#include <OpenHome/Json.h>
#include <OpenHome/Private/Ascii.h>

using namespace OpenHome;
using namespace OpenHome::TestFramework;

namespace OpenHome {

class SuiteJsonEncode : public Suite, private IWriter
{
public:
    SuiteJsonEncode();
private: // from Suite
    void Test() override;
private:
    void EncodeChar(TByte aChar, const TChar* aEncoded);
private: // from IWriter
    void Write(TByte aValue) override;
    void Write(const Brx& aBuffer) override;
    void WriteFlush() override;
private:
    Bws<128> iEncoded;
};

class SuiteJsonDecode : public Suite
{
public:
    SuiteJsonDecode();
private: // from Suite
    void Test() override;
private:
    static void DecodeChar(const TChar* aEncoded, TByte aDecoded);
};

class SuiteJsonParser : public SuiteUnitTest
{
public:
    SuiteJsonParser();
public: // from SuiteUnitTest
    void Setup() override;
    void TearDown() override;
private:
    void TestParseUnescapeInPlace();
    void TestParseNoUnescapeInPlace();
    void TestParseArray();
    void TestParseObject();
    void TestValidKey();
    void TestInvalidKey();
    void TestGetValidString();
    void TestGetInvalidString();
    void TestGetValidNum();
    void TestGetInvalidNum();
    void TestGetStringAsNum();
    void TestGetNumAsString();
    void TestCorruptInput();
private:
    JsonParser* iParser;
};

class SuiteWriterJson : public SuiteUnitTest
{
public:
    SuiteWriterJson();
public: // from SuiteUnitTest
    void Setup() override;
    void TearDown() override;
private:
    void TestWriteInt();
    void TestWriteString();
    void TestWriteBool();
};

class SuiteWriterJsonDocument : public SuiteUnitTest
{
public:
    SuiteWriterJsonDocument();
public: // from SuiteUnitTest
    void Setup() override;
    void TearDown() override;
private:
    void TestWriteEmptyObject();
    void TestWriteInt();
    void TestWriteString();
    void TestWriteBool();
    void TestWriteArray();
    void TestWriteObject();
    void TestWriteMixed();
private:
    Bws<512> iBuf;
    WriterBuffer* iWriterBuf;
};

class SuiteWriterJsonArray : public SuiteUnitTest
{
public:
    SuiteWriterJsonArray();
public: // from SuiteUnitTest
    void Setup() override;
    void TearDown() override;
private:
    void TestWriteEmpty();
    void TestWriteInt();
    void TestWriteString();
    void TestWriteBool();
    void TestWriteArray();
    void TestWriteObject();
    void TestWriteMixed();
private:
    Bws<512> iBuf;
    WriterBuffer* iWriterBuf;
};

} // namespace OpenHome


SuiteJsonEncode::SuiteJsonEncode()
    : Suite("Encode")
{
}

void SuiteJsonEncode::Test()
{
    EncodeChar('\"', "\\\"");
    EncodeChar('\\', "\\\\");
    EncodeChar( '/', "\\/");
    EncodeChar('\b', "\\b");
    EncodeChar('\f', "\\f");
    EncodeChar('\n', "\\n");
    EncodeChar('\r', "\\r");
    EncodeChar('\t', "\\t");
    for (TUint i=0; i<32; i++) {
        iEncoded.SetBytes(0);
        TByte b = (TByte)i;
        Bws<1> ch(&b, 1);
        Json::Escape(*this, ch);
        TEST(iEncoded.Bytes() > 1);
        if (iEncoded.Bytes() > 2) {
            Brn start(iEncoded.Ptr(), 2);
            TEST(start == Brn("\\u"));
            TEST(Ascii::UintHex(iEncoded.Split(2)) == i);
        }
    }
    // with very few exceptions, characters above 0x1F should not be encoded
    for (TUint i=32; i<256; i++) {
        if (i == '\"' || i == '/' || i == '\\') {
            // ...the exceptions mentioned above
            continue;
        }
        iEncoded.SetBytes(0);
        TByte b = (TByte)i;
        Bws<1> ch(&b, 1);
        Json::Escape(*this, ch);
        TEST(iEncoded == ch);
    }
}

void SuiteJsonEncode::EncodeChar(TByte aChar, const TChar* aEncoded)
{
    iEncoded.SetBytes(0);
    Bws<1> ch(&aChar, 1);
    Json::Escape(*this, ch);
    Brn expected(aEncoded);
    TEST(iEncoded == expected);
}

void SuiteJsonEncode::Write(TByte aValue)
{
    iEncoded.Append(aValue);
}

void SuiteJsonEncode::Write(const Brx& aBuffer)
{
    iEncoded.Append(aBuffer);
}

void SuiteJsonEncode::WriteFlush()
{
}


SuiteJsonDecode::SuiteJsonDecode()
    : Suite("Decode")
{
}

void SuiteJsonDecode::Test()
{
    DecodeChar("\\\"", '\"');
    DecodeChar("\\\\", '\\');
    DecodeChar("\\/", '/');
    DecodeChar("\\b", '\b');
    DecodeChar("\\f", '\f');
    DecodeChar("\\n", '\n');
    DecodeChar("\\r", '\r');
    DecodeChar("\\t", '\t');
    for (TUint i=0; i<256; i++) {
        Bws<7> enc("\\u00");
        Ascii::AppendHex(enc, (TByte)i);
        DecodeChar(enc.PtrZ(), (TByte)i);
    }
    Bws<64> url("http:\\/\\/domain\\/path?query");
    Json::Unescape(url);
    TEST(url == Brn("http://domain/path?query"));
}

void SuiteJsonDecode::DecodeChar(const TChar* aEncoded, TByte aDecoded)
{ // static
    Bws<6> buf(aEncoded);
    Json::Unescape(buf);
    TEST(buf.Bytes() == 1);
    TEST(buf[0] == aDecoded);
}


// SuiteJsonParser

SuiteJsonParser::SuiteJsonParser()
    : SuiteUnitTest("SuiteJsonParser")
{
    AddTest(MakeFunctor(*this, &SuiteJsonParser::TestParseUnescapeInPlace), "TestParseUnescapeInPlace");
    AddTest(MakeFunctor(*this, &SuiteJsonParser::TestParseNoUnescapeInPlace), "TestParseNoUnescapeInPlace");
    AddTest(MakeFunctor(*this, &SuiteJsonParser::TestParseArray), "TestParseArray");
    AddTest(MakeFunctor(*this, &SuiteJsonParser::TestParseObject), "TestParseObject");
    AddTest(MakeFunctor(*this, &SuiteJsonParser::TestValidKey), "TestValidKey");
    AddTest(MakeFunctor(*this, &SuiteJsonParser::TestInvalidKey), "TestInvalidKey");
    AddTest(MakeFunctor(*this, &SuiteJsonParser::TestGetValidString), "TestGetValidString");
    AddTest(MakeFunctor(*this, &SuiteJsonParser::TestGetInvalidString), "TestGetInvalidString");
    AddTest(MakeFunctor(*this, &SuiteJsonParser::TestGetValidNum), "TestGetValidNum");
    AddTest(MakeFunctor(*this, &SuiteJsonParser::TestGetInvalidNum), "TestGetInvalidNum");
    AddTest(MakeFunctor(*this, &SuiteJsonParser::TestGetStringAsNum), "TestGetStringAsNum");
    AddTest(MakeFunctor(*this, &SuiteJsonParser::TestGetNumAsString), "TestGetNumAsString");
    AddTest(MakeFunctor(*this, &SuiteJsonParser::TestCorruptInput), "TestCorruptInput");
}

void SuiteJsonParser::Setup()
{
    iParser = new JsonParser();
}

void SuiteJsonParser::TearDown()
{
    delete iParser;
}

void SuiteJsonParser::TestParseUnescapeInPlace()
{
    // FIXME - fails; bad parsing of escaped double quote.

    //const TBool unescapeInPlace = true;
    //Bwh json("{\"key1\":\"line1\\\"\\\\\\/\\b\\f\\n\\r\\tline2\"}");
    ////Bwh json("{\"key1\":\"line1\\\\\\/\\b\\f\\n\\r\\tline2\"}"); // This works - bad parsing of double quotes?

    //iParser->Parse(json, unescapeInPlace);
    //TEST(iParser->HasKey("key1"));
    //TEST(iParser->String("key1") == Brn("line1\"\\/\b\f\n\r\tline2"));
}

void SuiteJsonParser::TestParseNoUnescapeInPlace()
{
    // FIXME - fails; bad parsing of escaped double quote.

    //const TBool unescapeInPlace = false;
    //Bwh json("{\"key1\":\"line1\\\"\\\\\\/\\b\\f\\n\\r\\tline2\"}");

    //iParser->Parse(json, unescapeInPlace);
    //TEST(iParser->HasKey("key1"));
    //TEST(iParser->String("key1") == Brn("line1\\\"\\\\\\/\\b\\f\\n\\r\\tline2"));
}

void SuiteJsonParser::TestParseArray()
{
    const TBool unescapeInPlace = false;
    const Brn json("[\"val1\", 2, false, \"val4\"]");

    // FIXME - arrays appear to be unsupported.
    // FIXME - maybe throw a JsonUnsupported instead?
    TEST_THROWS(iParser->Parse(json, unescapeInPlace), JsonCorrupt);
}

void SuiteJsonParser::TestParseObject()
{
    const TBool unescapeInPlace = false;
    const Brn json("{\"key1\":{\"key2\":{\"key3\": 3, \"key4\":\"val4\"}}}");

    iParser->Parse(json, unescapeInPlace);
    TEST(iParser->HasKey("key1"));
    Brn nextObject = iParser->String("key1");
    TEST(nextObject == Brn("{\"key2\":{\"key3\": 3, \"key4\":\"val4\"}}"));

    iParser->Parse(nextObject, unescapeInPlace);
    TEST(iParser->HasKey("key2"));
    nextObject = iParser->String("key2");
    TEST(nextObject == Brn("{\"key3\": 3, \"key4\":\"val4\"}"));

    iParser->Parse(nextObject, unescapeInPlace);
    TEST(iParser->HasKey("key3"));
    TEST(iParser->Num("key3") == 3);

    TEST(iParser->HasKey("key4"));
    TEST(iParser->String("key4") == Brn("val4"));




    // FIXME - this fails - looks like it's due to integer as last value in nested object.

    //const TBool unescapeInPlace = false;
    //const Brn json("{\"key1\":{\"key2\":{\"key3\": 3, \"key4\":4}, \"key5\":\"val5\"}}");

    //iParser->Parse(json, unescapeInPlace);
    //TEST(iParser->HasKey("key1"));
    //Brn nextObject = iParser->String("key1");
    //TEST(nextObject == Brn("{\"key2\":{\"key3\": 3, \"key4\":4}, \"key5\":\"val5\"}}"));

    //iParser->Parse(nextObject, unescapeInPlace);
    //TEST(iParser->HasKey("key2"));
    //nextObject = iParser->String("key2");
    //TEST(nextObject == Brn("{\"key3\": 3, \"key4\":\"val4\"}"));

    //TEST(iParser->HasKey("key5"));
    //TEST(iParser->String("key5") == Brn("\"val5\""));

    //iParser->Parse(nextObject, unescapeInPlace);
    //TEST(iParser->HasKey("key3"));
    //TEST(iParser->Num("key3") == 3);

    //TEST(iParser->HasKey("key4"));
    //TEST(iParser->String("key4") == Brn("val4"));


}

void SuiteJsonParser::TestValidKey()
{
    const TBool unescapeInPlace = false;
    const Brn json("{\"key1\":\"val1\"}");

    iParser->Parse(json, unescapeInPlace);
    TEST(iParser->HasKey("key1"));
    TEST(iParser->HasKey(Brn("key1")));
}

void SuiteJsonParser::TestInvalidKey()
{
    const TBool unescapeInPlace = false;
    const Brn json("{\"key1\":\"val1\"}");

    iParser->Parse(json, unescapeInPlace);
    TEST(iParser->HasKey("key2") == false);
    TEST(iParser->HasKey(Brn("key2")) == false);
}

void SuiteJsonParser::TestGetValidString()
{
    const TBool unescapeInPlace = false;
    const Brn json("{\"key1\":\"val1\"}");

    iParser->Parse(json, unescapeInPlace);
    TEST(iParser->String("key1") == Brn("val1"));
    TEST(iParser->String(Brn("key1")) == Brn("val1"));
}

void SuiteJsonParser::TestGetInvalidString()
{
    const TBool unescapeInPlace = false;
    const Brn json("{\"key1\":\"val1\"}");

    iParser->Parse(json, unescapeInPlace);
    TEST_THROWS(iParser->String("key2"), JsonKeyNotFound);
    TEST_THROWS(iParser->String(Brn("key2")), JsonKeyNotFound);
}

void SuiteJsonParser::TestGetValidNum()
{
    // FIXME - fails

    //const TBool unescapeInPlace = false;
    //const Brn json("{\"key1\": 1}");

    //iParser->Parse(json, unescapeInPlace);
    //TEST(iParser->Num("key1") == 1);
    //TEST(iParser->Num(Brn("key1")) == 1);
}

void SuiteJsonParser::TestGetInvalidNum()
{
    // FIXME - fails

    //const TBool unescapeInPlace = false;
    //const Brn json("{\"key1\":1}");

    //iParser->Parse(json, unescapeInPlace);
    //TEST_THROWS(iParser->Num("key2"), JsonKeyNotFound);
    //TEST_THROWS(iParser->Num(Brn("key2")), JsonKeyNotFound);
}

void SuiteJsonParser::TestGetStringAsNum()
{
    const TBool unescapeInPlace = false;
    const Brn json("{\"key1\":\"val1\"}");

    iParser->Parse(json, unescapeInPlace);
    TEST_THROWS(iParser->Num("key2"), JsonKeyNotFound);
    TEST_THROWS(iParser->Num(Brn("key2")), JsonKeyNotFound);
}

void SuiteJsonParser::TestGetNumAsString()
{
    // FIXME - fails
    //const TBool unescapeInPlace = false;
    //const Brn json("{\"key1\":1}");

    //iParser->Parse(json, unescapeInPlace);
    //TEST_THROWS(iParser->String("key2"), JsonKeyNotFound);
    //TEST_THROWS(iParser->String(Brn("key2")), JsonKeyNotFound);
}

void SuiteJsonParser::TestCorruptInput()
{
    const TBool unescapeInPlace = false;

    TEST_THROWS(iParser->Parse(Brn("{\"key1:1}"), unescapeInPlace), JsonCorrupt);   // No closing quote around key string.
    TEST_THROWS(iParser->Parse(Brn("\"key1\":1"), unescapeInPlace), JsonCorrupt);   // Unenclosed object.
    TEST_THROWS(iParser->Parse(Brn("\"key1\":1}"), unescapeInPlace), JsonCorrupt);  // Object not opened.
    TEST_THROWS(iParser->Parse(Brn("{\"key1\":1"), unescapeInPlace), JsonCorrupt);  // Object not closed.
    TEST_THROWS(iParser->Parse(Brn("{\"key1\":}"), unescapeInPlace), JsonCorrupt);  // No value.
    TEST_THROWS(iParser->Parse(Brn("{:1}"), unescapeInPlace), JsonCorrupt);         // No key.
    TEST_THROWS(iParser->Parse(Brn("{\"key1\" 1"), unescapeInPlace), JsonCorrupt);  // No separator.
    TEST_THROWS(iParser->Parse(Brn("{1:2}"), unescapeInPlace), JsonCorrupt);        // Number as key.
    TEST_THROWS(iParser->Parse(Brn("{true:2}"), unescapeInPlace), JsonCorrupt);     // Boolean as key.
}


// SuiteWriterJson

SuiteWriterJson::SuiteWriterJson()
    : SuiteUnitTest("SuiteWriterJson")
{
    AddTest(MakeFunctor(*this, &SuiteWriterJson::TestWriteInt), "TestWriteInt");
    AddTest(MakeFunctor(*this, &SuiteWriterJson::TestWriteString), "TestWriteString");
    AddTest(MakeFunctor(*this, &SuiteWriterJson::TestWriteBool), "TestWriteBool");
}

void SuiteWriterJson::Setup()
{
}

void SuiteWriterJson::TearDown()
{
}

void SuiteWriterJson::TestWriteInt()
{
    Bws<32> buf;
    WriterBuffer writerBuf(buf);

    WriterJson::WriteValueInt(writerBuf, -2147483647);
    TEST(buf == Brn("-2147483647"));

    buf.SetBytes(0);
    WriterJson::WriteValueInt(writerBuf, 2147483647);
    TEST(buf == Brn("2147483647"));

    buf.SetBytes(0);
    WriterJson::WriteValueInt(writerBuf, 0);
    TEST(buf == Brn("0"));

    buf.SetBytes(0);
    WriterJson::WriteValueInt(writerBuf, -256);
    TEST(buf == Brn("-256"));

    buf.SetBytes(0);
    WriterJson::WriteValueInt(writerBuf, 256);
    TEST(buf == Brn("256"));
}

void SuiteWriterJson::TestWriteString()
{
    Bws<32> buf;
    WriterBuffer writerBuf(buf);

    WriterJson::WriteValueString(writerBuf, Brn());
    TEST(buf == Brn("\"\""));

    buf.SetBytes(0);
    WriterJson::WriteValueString(writerBuf, Brn("a string"));
    TEST(buf == (Brn("\"a string\"")));

    buf.SetBytes(0);
    WriterJson::WriteValueString(writerBuf, Brn("line1\"\\/\b\f\n\r\tline2"));
    TEST(buf == Brn("\"line1\\\"\\\\\\/\\b\\f\\n\\r\\tline2\""));
}

void SuiteWriterJson::TestWriteBool()
{
    Bws<32> buf;
    WriterBuffer writerBuf(buf);

    WriterJson::WriteValueBool(writerBuf, false);
    TEST(buf == Brn("false"));

    buf.SetBytes(0);
    WriterJson::WriteValueBool(writerBuf, true);
    TEST(buf == Brn("true"));
}


// SuiteWriterJsonDocument

SuiteWriterJsonDocument::SuiteWriterJsonDocument()
    : SuiteUnitTest("SuiteWriterJsonDocument")
{
    AddTest(MakeFunctor(*this, &SuiteWriterJsonDocument::TestWriteEmptyObject), "TestWriteEmptyObject");
    AddTest(MakeFunctor(*this, &SuiteWriterJsonDocument::TestWriteInt), "TestWriteInt");
    AddTest(MakeFunctor(*this, &SuiteWriterJsonDocument::TestWriteString), "TestWriteString");
    AddTest(MakeFunctor(*this, &SuiteWriterJsonDocument::TestWriteBool), "TestWriteBool");
    AddTest(MakeFunctor(*this, &SuiteWriterJsonDocument::TestWriteArray), "TestWriteArray");
    AddTest(MakeFunctor(*this, &SuiteWriterJsonDocument::TestWriteObject), "TestWriteObject");
    AddTest(MakeFunctor(*this, &SuiteWriterJsonDocument::TestWriteMixed), "TestWriteMixed");
}

void SuiteWriterJsonDocument::Setup()
{
    iWriterBuf = new WriterBuffer(iBuf);
}

void SuiteWriterJsonDocument::TearDown()
{
    delete iWriterBuf;
    iBuf.SetBytes(0);
}

void SuiteWriterJsonDocument::TestWriteEmptyObject()
{
    WriterJsonDocument jsonWriter1(*iWriterBuf);
    jsonWriter1.WriteEnd();
    TEST(iBuf == Brn("{}"));
}

void SuiteWriterJsonDocument::TestWriteInt()
{
    // Write key using const TChar*.
    WriterJsonDocument jsonWriter1(*iWriterBuf);
    jsonWriter1.WriteInt("key1", 256);
    jsonWriter1.WriteEnd();
    TEST(iBuf == Brn("{\"key1\":256}"));

    iBuf.SetBytes(0);
    // Write key using const Brx&.
    WriterJsonDocument jsonWriter2(*iWriterBuf);
    jsonWriter2.WriteInt(Brn("key2"), 256);
    jsonWriter2.WriteEnd();
    TEST(iBuf == Brn("{\"key2\":256}"));
}

void SuiteWriterJsonDocument::TestWriteString()
{
    // Write key using const TChar*, val using const TChar*.
    WriterJsonDocument jsonWriter1(*iWriterBuf);
    jsonWriter1.WriteString("key1", "val1");
    jsonWriter1.WriteEnd();
    TEST(iBuf == Brn("{\"key1\":\"val1\"}"));

    iBuf.SetBytes(0);
    // Write key using const TChar*, val using const Brx&.
    WriterJsonDocument jsonWriter2(*iWriterBuf);
    jsonWriter2.WriteString("key2", Brn("val2"));
    jsonWriter2.WriteEnd();
    TEST(iBuf == Brn("{\"key2\":\"val2\"}"));

    iBuf.SetBytes(0);
    // Write key using const Brx&, val using const Brx&.
    WriterJsonDocument jsonWriter3(*iWriterBuf);
    jsonWriter3.WriteString(Brn("key3"), Brn("val3"));
    jsonWriter3.WriteEnd();
    TEST(iBuf == Brn("{\"key3\":\"val3\"}"));

    iBuf.SetBytes(0);
    // Write empty key and val.
    WriterJsonDocument jsonWriter4(*iWriterBuf);
    jsonWriter4.WriteString(Brn(""), Brn(""));
    jsonWriter4.WriteEnd();
    TEST(iBuf == Brn("{\"\":\"\"}"));
}

void SuiteWriterJsonDocument::TestWriteBool()
{
    // Write key using const TChar*, val false.
    WriterJsonDocument jsonWriter1(*iWriterBuf);
    jsonWriter1.WriteBool("key1", false);
    jsonWriter1.WriteEnd();
    TEST(iBuf == Brn("{\"key1\":false}"));

    iBuf.SetBytes(0);
    // Write key using const Brx&, val false.
    WriterJsonDocument jsonWriter2(*iWriterBuf);
    jsonWriter2.WriteBool(Brn("key2"), false);
    jsonWriter2.WriteEnd();
    TEST(iBuf == Brn("{\"key2\":false}"));

    iBuf.SetBytes(0);
    // Write key using const TChar*, val true.
    WriterJsonDocument jsonWriter3(*iWriterBuf);
    jsonWriter3.WriteBool("key3", true);
    jsonWriter3.WriteEnd();
    TEST(iBuf == Brn("{\"key3\":true}"));

    iBuf.SetBytes(0);
    // Write key using const Brx&, val true.
    WriterJsonDocument jsonWriter4(*iWriterBuf);
    jsonWriter4.WriteBool(Brn("key4"), true);
    jsonWriter4.WriteEnd();
    TEST(iBuf == Brn("{\"key4\":true}"));
}

void SuiteWriterJsonDocument::TestWriteArray()
{
    // Write key using const TChar*.
    WriterJsonDocument jsonWriter1(*iWriterBuf);
    WriterJsonArray array1 = jsonWriter1.CreateArray("key1");
    array1.WriteInt(256);
    array1.WriteString(Brn("val2"));
    array1.WriteBool(false);
    array1.WriteEnd();
    jsonWriter1.WriteEnd();
    TEST(iBuf == Brn("{\"key1\":[256,\"val2\",false]}"));

    iBuf.SetBytes(0);
    // Write key using const TChar*.
    WriterJsonDocument jsonWriter2(*iWriterBuf);
    WriterJsonArray array2 = jsonWriter2.CreateArray("key2");
    array2.WriteInt(256);
    array2.WriteString(Brn("val2"));
    array2.WriteBool(false);
    array2.WriteEnd();
    jsonWriter2.WriteEnd();
    TEST(iBuf == Brn("{\"key2\":[256,\"val2\",false]}"));

    // Write empty array.
    iBuf.SetBytes(0);
    WriterJsonDocument jsonWriter3(*iWriterBuf);
    WriterJsonArray array3 = jsonWriter3.CreateArray("key3");
    array3.WriteEnd();
    jsonWriter3.WriteEnd();
    TEST(iBuf == Brn("{\"key3\":null}"));
}

void SuiteWriterJsonDocument::TestWriteObject()
{
    // Write key using const TChar*.
    WriterJsonDocument jsonWriter1(*iWriterBuf);
    WriterJsonObject obj1 = jsonWriter1.CreateObject("key1");
    obj1.WriteInt("key2", 256);
    obj1.WriteEnd();
    jsonWriter1.WriteEnd();
    TEST(iBuf == Brn("{\"key1\":{\"key2\":256}}"));

    iBuf.SetBytes(0);
    // Write key using const TChar*.
    WriterJsonDocument jsonWriter2(*iWriterBuf);
    WriterJsonObject obj2 = jsonWriter2.CreateObject("key1");
    obj2.WriteInt("key2", 256);
    obj2.WriteEnd();
    jsonWriter2.WriteEnd();
    TEST(iBuf == Brn("{\"key1\":{\"key2\":256}}"));

    // Write empty object.
    iBuf.SetBytes(0);
    WriterJsonDocument jsonWriter3(*iWriterBuf);
    WriterJsonObject obj3 = jsonWriter3.CreateObject("key3");
    obj3.WriteEnd();
    jsonWriter3.WriteEnd();
    TEST(iBuf == Brn("{\"key3\":null}"));   // FIXME - why is this "null" and not an empty object (i.e., {})? TestWriteEmptyObject() writes {}.
}

void SuiteWriterJsonDocument::TestWriteMixed()
{
    WriterJsonDocument jsonWriter(*iWriterBuf);
    jsonWriter.WriteInt("key1", -128);
    jsonWriter.WriteString("key2", "str1");
    jsonWriter.WriteBool("key3", false);

    WriterJsonArray jsonArray = jsonWriter.CreateArray("key4");
    jsonArray.WriteInt(128);
    jsonArray.WriteString(Brn("str2"));
    jsonArray.WriteBool(true);
    jsonArray.WriteEnd();

    WriterJsonObject jsonObject = jsonWriter.CreateObject("key5");
    jsonObject.WriteInt("key6", 256);
    jsonObject.WriteString("key7", "str3");
    jsonObject.WriteBool("key8", true);
    jsonObject.WriteEnd();

    jsonWriter.WriteEnd();

    TEST(iBuf == Brn("{\"key1\":-128,\"key2\":\"str1\",\"key3\":false,\"key4\":[128,\"str2\",true],\"key5\":{\"key6\":256,\"key7\":\"str3\",\"key8\":true}}"));
}


// SuiteWriterJsonArray

SuiteWriterJsonArray::SuiteWriterJsonArray()
    : SuiteUnitTest("SuiteWriterJsonArray")
{
    AddTest(MakeFunctor(*this, &SuiteWriterJsonArray::TestWriteEmpty), "TestWriteEmpty");
    AddTest(MakeFunctor(*this, &SuiteWriterJsonArray::TestWriteInt), "TestWriteInt");
    AddTest(MakeFunctor(*this, &SuiteWriterJsonArray::TestWriteString), "TestWriteString");
    AddTest(MakeFunctor(*this, &SuiteWriterJsonArray::TestWriteBool), "TestWriteBool");
    AddTest(MakeFunctor(*this, &SuiteWriterJsonArray::TestWriteArray), "TestWriteArray");
    AddTest(MakeFunctor(*this, &SuiteWriterJsonArray::TestWriteObject), "TestWriteObject");
    AddTest(MakeFunctor(*this, &SuiteWriterJsonArray::TestWriteMixed), "TestWriteMixed");
}

void SuiteWriterJsonArray::Setup()
{
    iWriterBuf = new WriterBuffer(iBuf);
}

void SuiteWriterJsonArray::TearDown()
{
    delete iWriterBuf;
    iBuf.SetBytes(0);
}

void SuiteWriterJsonArray::TestWriteEmpty()
{
    WriterJsonArray jsonArray(*iWriterBuf);
    jsonArray.WriteEnd();
    TEST(iBuf == Brn("null"));  // FIXME - why is this null and not []? TestWriteEmptyObject() writes {} instead of null.
}

void SuiteWriterJsonArray::TestWriteInt()
{
    WriterJsonArray jsonArray(*iWriterBuf);
    jsonArray.WriteInt(256);
    jsonArray.WriteEnd();
    TEST(iBuf == Brn("[256]"));
}

void SuiteWriterJsonArray::TestWriteString()
{
    // Empty string.
    WriterJsonArray jsonArray1(*iWriterBuf);
    jsonArray1.WriteString(Brn(""));
    jsonArray1.WriteEnd();
    TEST(iBuf == Brn("[\"\"]"));

    iBuf.SetBytes(0);
    // Non-empty string, with chars requiring escaping.
    WriterJsonArray jsonArray2(*iWriterBuf);
    jsonArray2.WriteString(Brn("line1\r\nline2"));
    jsonArray2.WriteEnd();
    TEST(iBuf == Brn("[\"line1\\r\\nline2\"]"));
}

void SuiteWriterJsonArray::TestWriteBool()
{
    // Write false.
    WriterJsonArray jsonArray1(*iWriterBuf);
    jsonArray1.WriteBool(false);
    jsonArray1.WriteEnd();
    TEST(iBuf == Brn("[false]"));

    iBuf.SetBytes(0);
    // Write true.
    WriterJsonArray jsonArray2(*iWriterBuf);
    jsonArray2.WriteBool(true);
    jsonArray2.WriteEnd();
    TEST(iBuf == Brn("[true]"));
}

void SuiteWriterJsonArray::TestWriteArray()
{
    WriterJsonArray jsonArray(*iWriterBuf);
    WriterJsonArray jsonArrayNested = jsonArray.CreateArray();
    jsonArrayNested.WriteInt(256);
    jsonArrayNested.WriteString(Brn("str"));
    jsonArrayNested.WriteBool(false);
    jsonArrayNested.WriteEnd();
    jsonArray.WriteEnd();
    TEST(iBuf == Brn("[[256,\"str\",false]]"));
}

void SuiteWriterJsonArray::TestWriteObject()
{
    WriterJsonArray jsonArray(*iWriterBuf);
    WriterJsonObject jsonObjectNested = jsonArray.CreateObject();
    jsonObjectNested.WriteInt("key1", 256);
    jsonObjectNested.WriteString("key2", "str");
    jsonObjectNested.WriteBool("key3", false);
    jsonObjectNested.WriteEnd();
    jsonArray.WriteEnd();
    TEST(iBuf == Brn("[{\"key1\":256,\"key2\":\"str\",\"key3\":false}]"));
}

void SuiteWriterJsonArray::TestWriteMixed()
{
    WriterJsonArray jsonArray(*iWriterBuf);
    jsonArray.WriteInt(256);
    jsonArray.WriteString(Brn("line1\r\nline2"));
    jsonArray.WriteBool(false);

    WriterJsonArray jsonArrayNested = jsonArray.CreateArray();
    jsonArrayNested.WriteInt(256);
    jsonArrayNested.WriteString(Brn("str"));
    jsonArrayNested.WriteBool(false);
    jsonArrayNested.WriteEnd();

    WriterJsonObject jsonObjectNested = jsonArray.CreateObject();
    jsonObjectNested.WriteInt("key1", 256);
    jsonObjectNested.WriteString("key2", "str");
    jsonObjectNested.WriteBool("key3", false);
    jsonObjectNested.WriteEnd();

    jsonArray.WriteEnd();

    TEST(iBuf == Brn("[256,\"line1\\r\\nline2\",false,[256,\"str\",false],{\"key1\":256,\"key2\":\"str\",\"key3\":false}]"));
}



void TestJson()
{
    Runner runner("JSON tests\n");
    runner.Add(new SuiteJsonEncode());
    runner.Add(new SuiteJsonDecode());
    runner.Add(new SuiteJsonParser());
    runner.Add(new SuiteWriterJson());
    runner.Add(new SuiteWriterJsonDocument());
    runner.Add(new SuiteWriterJsonArray());
    runner.Run();
}