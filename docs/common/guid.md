# GUID Registry {#common_guids}

This page records stable foliOS GUID and UUID namespace assignments that cross
component boundaries.

## UUIDv5 Namespace

foliOS UUIDv5 values use this namespace unless a narrower protocol explicitly
defines another namespace:

| Namespace | Value |
| --- | --- |
| foliOS UUIDv5 | `3E30B1DB-32E4-4CD5-999C-02ADFE52C417` |

## GPT Partition Types

The disk image tooling uses UUIDv5-derived GPT partition type GUIDs.

| Type | Name input | GUID |
| --- | --- | --- |
| Boot | `strata/gpt/boot` | `3C874624-2615-5FB3-87D4-771FA8355D02` |
| Data | `strata/gpt/data` | `DFA197A2-4C9D-57FF-A7CB-8A9038E10B0D` |
| Swap | `strata/gpt/swap` | `9FC2A4C2-2250-5095-AA89-74572A260833` |
