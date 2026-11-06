#include <catch2/catch_test_macros.hpp>
#include <array>
#include "engine/NoteMap.h"

using namespace dg;

TEST_CASE("a new note map has no assignments", "[notemap]")
{
    NoteMap map;
    for (int n : { 0, 36, 127 })
        CHECK(map.slotForNote(n) == -1);
    for (int s = 0; s < kNumSlots; ++s)
        CHECK(map.noteForSlot(s) == -1);
}

TEST_CASE("assign links note and pad in both directions", "[notemap]")
{
    NoteMap map;
    map.assign(2, 40);
    CHECK(map.slotForNote(40) == 2);
    CHECK(map.noteForSlot(2) == 40);
}

TEST_CASE("reassigning a pad frees its previous note", "[notemap]")
{
    NoteMap map;
    map.assign(2, 40);
    map.assign(2, 41);
    CHECK(map.slotForNote(40) == -1);
    CHECK(map.slotForNote(41) == 2);
}

TEST_CASE("a note taken by another pad leaves its previous owner", "[notemap]")
{
    NoteMap map;
    map.assign(0, 40);
    map.assign(5, 40);
    CHECK(map.slotForNote(40) == 5);
    CHECK(map.noteForSlot(0) == -1);
}

TEST_CASE("assign with -1 frees the pad", "[notemap]")
{
    NoteMap map;
    map.assign(3, 50);
    map.assign(3, -1);
    CHECK(map.slotForNote(50) == -1);
    CHECK(map.noteForSlot(3) == -1);
}

TEST_CASE("out-of-range notes and slots are ignored", "[notemap]")
{
    NoteMap map;
    map.assign(1, 60);
    const auto before = map.notes();
    CHECK(map.slotForNote(-1) == -1);
    CHECK(map.slotForNote(128) == -1);
    map.assign(16, 40);
    map.assign(-1, 40);
    map.assign(0, 128);
    CHECK(map.notes() == before);
}

TEST_CASE("setAll applies a full profile and keeps the first pad on duplicates", "[notemap]")
{
    NoteMap map;
    const std::array<int, kNumSlots> in { 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 36 };
    map.setAll(in);
    CHECK(map.noteForSlot(15) == -1);
    CHECK(map.slotForNote(36) == 0);
    auto expected = in;
    expected[15] = -1;
    CHECK(map.notes() == expected);
}
