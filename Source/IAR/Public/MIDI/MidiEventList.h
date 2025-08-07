// -------------------------------------------------------------------------------
// Copyright 2025 William Wolff. All Rights Reserved.
// This code is property of William Wolff and protected by copywright law.
// Proibited copy or distribution without expressed authorization of the Author.
// Creation: 05/08/2025
// Author  : William Wolff
// -------------------------------------------------------------------------------
#ifndef _MIDIEVENTLIST_H_INCLUDED
#define _MIDIEVENTLIST_H_INCLUDED

#include "MidiEvent.h"

#include <vector>


namespace smf {

class MidiEventList {
	public:
		                 MidiEventList      (void);
		                 MidiEventList      (const MidiEventList& other);
		                 MidiEventList      (MidiEventList&& other);

		                ~MidiEventList      ();

		MidiEventList&   operator=          (MidiEventList& other);
		MidiEvent&       operator[]         (int index);
		const MidiEvent& operator[]         (int index) const;
		MidiEvent&       at                 (int index);
		const MidiEvent& at                 (int index) const;

		MidiEvent&       back               (void);
		const MidiEvent& back               (void) const;
		MidiEvent&       last               (void);
		const MidiEvent& last               (void) const;
		MidiEvent&       getEvent           (int index);
		const MidiEvent& getEvent           (int index) const;
		void             clear              (void);
		void             reserve            (int rsize);
		int              getEventCount      (void) const;
		int              getSize            (void) const;
		int              size               (void) const;
		void             removeEmpties      (void);
		int              linkNotePairsFIFO  (void);
		int              linkNotePairsLIFO  (void);
		int              linkNotePairs      (void) { return linkNotePairsFIFO(); }
		int              linkEventPairs     (void);
		void             clearLinks         (void);
		void             clearSequence      (void);
		int              markSequence       (int sequence = 1);

		int              push               (MidiEvent& event);
		int              push_back          (MidiEvent& event);
		int              append             (MidiEvent& event);

		// careful when using these, intended for internal use in MidiFile class:
		void             detach             (void);
		int              push_back_no_copy  (MidiEvent* event);

		// access to the list of MidiEvents for sorting with an external function:
		MidiEvent**      data               (void);

	protected:
		std::vector<MidiEvent*> list;

	private:
		void             sort                   (void) { return sortNoteOnsBeforeOffs(); }
		void             sortNoteOnsBeforeOffs  (void);
		void             sortNoteOffsBeforeOns  (void);

	// MidiFile class calls sort()
	friend class MidiFile;

	static int eventCompareNoteOffsBeforeOns(const void* a, const void* b);
	static int eventCompareNoteOnsBeforeOffs(const void* a, const void* b);
	static int eventCompare(const void* a, const void* b) { return eventCompareNoteOnsBeforeOffs(a, b); }
};


} // end of namespace smf

#endif /* _MIDIEVENTLIST_H_INCLUDED */



