/*
 * read_converter.hpp
 *
 *  Created on: Apr 13, 2012
 *      Author: andrey
 */

#ifndef READ_CONVERTER_HPP_
#define READ_CONVERTER_HPP_

#include <fstream>

#include "io/binary_io.hpp"
#include "io/rc_reader_wrapper.hpp"
#include "dataset_readers.hpp"
#include "simple_tools.hpp"

namespace debruijn_graph {

typedef io::IReader<io::SingleReadSeq> SequenceSingleReadStream;
typedef io::IReader<io::PairedReadSeq> SequencePairedReadStream;

void convert_reads_to_binary() {

    if (fileExists(cfg::get().temp_bin_reads_info)) {
        std::ifstream info;
        info.open(cfg::get().temp_bin_reads_info.c_str(), std::ios_base::in);
        size_t thread_num;
        info >> thread_num;
        info.close();

        if (thread_num == cfg::get().thread_number) {
            INFO("Binary reads detected");
            return;
        }
    }

    INFO("Converting paired reads to binary format (takes a while)");
    auto_ptr<PairedReadStream> paired_reader = paired_easy_reader(false, 0);
    io::BinaryWriter paired_converter(cfg::get().paired_read_prefix, cfg::get().thread_number, cfg::get().buffer_size);
    paired_converter.ToBinary(*paired_reader);

    INFO("Converting single reads to binary format (takes a while)");
    auto_ptr<SingleReadStream> single_reader = single_easy_reader(false, false);
    io::BinaryWriter single_converter(cfg::get().single_read_prefix, cfg::get().thread_number, cfg::get().buffer_size);
    single_converter.ToBinary(*single_reader);

    std::ofstream info;
    info.open(cfg::get().temp_bin_reads_info.c_str(), std::ios_base::out);
    info << cfg::get().thread_number;
    info.close();
}


std::vector< SequenceSingleReadStream* > apply_single_wrappers(bool followed_by_rc,
        std::vector< SequenceSingleReadStream* >& single_readers,
        std::vector< SequencePairedReadStream* > * paired_readers = 0) {

    VERIFY(single_readers.size() != 0);
    size_t size = single_readers.size();
    std::vector<SequenceSingleReadStream*> raw_readers(size);

    if (paired_readers != 0) {
        VERIFY(single_readers.size() == paired_readers->size());

        for (size_t i = 0; i < size; ++i) {
            SequenceSingleReadStream * single_stream = single_readers.at(i);
            SequencePairedReadStream * paired_stream = paired_readers->at(i);
            io::SeqSingleReadStreamWrapper * single_wrapper = new io::SeqSingleReadStreamWrapper(*paired_stream);

            raw_readers[i] = new io::MultifileReader<io::SingleReadSeq>(*single_wrapper, *single_stream);
        }
    }
    else {
       for (size_t i = 0; i < size; ++i) {
           raw_readers[i] = single_readers.at(i);
       }
    }

    if (followed_by_rc) {
        std::vector<SequenceSingleReadStream*> rc_readers(size);
        for (size_t i = 0; i < size; ++i) {
            rc_readers[i] = new io::RCReaderWrapper<io::SingleReadSeq>(*raw_readers[i]);
        }
        return rc_readers;
    } else {
        return raw_readers;
    }
}


std::vector< SequencePairedReadStream* > apply_paired_wrappers(bool followed_by_rc,
        std::vector< SequencePairedReadStream* >& paired_readers) {

    VERIFY(paired_readers.size() != 0);
    size_t size = paired_readers.size();

    if (followed_by_rc) {
        std::vector<SequencePairedReadStream*> rc_readers(size);
        for (size_t i = 0; i < size; ++i) {
            rc_readers[i] = new io::RCReaderWrapper<io::PairedReadSeq>(*paired_readers[i]);
        }
        return rc_readers;
    } else {
        return paired_readers;
    }
}


std::vector< SequenceSingleReadStream* > single_binary_readers(bool followed_by_rc, bool including_paired_reads) {

    std::vector<SequenceSingleReadStream*> single_streams(cfg::get().thread_number);
    for (size_t i = 0; i < cfg::get().thread_number; ++i) {
        single_streams[i] = new io::SeqSingleReadStream(cfg::get().single_read_prefix, i);
    }

    if (including_paired_reads) {
        std::vector<SequencePairedReadStream*> paired_streams(cfg::get().thread_number);
        for (size_t i = 0; i < cfg::get().thread_number; ++i) {
            paired_streams[i] = new io::SeqPairedReadStream(cfg::get().paired_read_prefix, i, 0);
        }
        return apply_single_wrappers(followed_by_rc, single_streams, &paired_streams);
    }
    else {
        return apply_single_wrappers(followed_by_rc, single_streams);
    }
}


std::vector< SequencePairedReadStream* > paired_binary_readers(bool followed_by_rc, size_t insert_size) {
    std::vector<SequencePairedReadStream*> paired_streams(cfg::get().thread_number);
    for (size_t i = 0; i < cfg::get().thread_number; ++i) {
        paired_streams[i] = new io::SeqPairedReadStream(cfg::get().paired_read_prefix, i, insert_size);
    }
    return apply_paired_wrappers(followed_by_rc, paired_streams);
}

auto_ptr<SequenceSingleReadStream> single_binary_multireader(bool followed_by_rc, bool including_paired_reads) {
    return auto_ptr<SequenceSingleReadStream>(new io::MultifileReader<io::SingleReadSeq>(single_binary_readers(followed_by_rc, including_paired_reads)));
}

auto_ptr<SequencePairedReadStream> paired_binary_multireader(bool followed_by_rc, size_t insert_size) {
    return auto_ptr<SequencePairedReadStream>(new io::MultifileReader<io::PairedReadSeq>(paired_binary_readers(followed_by_rc, insert_size)));
}


class BufferedReadersStorage {

private:

    std::vector< SequenceSingleReadStream* > * single_streams_;

    std::vector< SequencePairedReadStream* > * paired_streams_;

    BufferedReadersStorage() {
        INFO("Creating buffered read storage");

        INFO("Buffering single reads... (takes a while)");
        single_streams_ = new std::vector< SequenceSingleReadStream* >(cfg::get().thread_number);
        for (size_t i = 0; i < cfg::get().thread_number; ++i) {
            io::PredictableIReader<io::SingleReadSeq> * s_stream = new io::SeqSingleReadStream(cfg::get().single_read_prefix, i);
            single_streams_->at(i) = new io::ReadBufferedStream<io::SingleReadSeq> (*s_stream);
        }

        INFO("Buffering paired reads... (takes a while)");
        paired_streams_ = new std::vector< SequencePairedReadStream* >(cfg::get().thread_number);
        for (size_t i = 0; i < cfg::get().thread_number; ++i) {
            io::PredictableIReader<io::PairedReadSeq> * p_stream = new io::SeqPairedReadStream(cfg::get().paired_read_prefix, i, 0);
            paired_streams_->at(i) = new io::ReadBufferedStream<io::PairedReadSeq> (*p_stream);
        }
    }

    BufferedReadersStorage(const BufferedReadersStorage&);

    BufferedReadersStorage& operator=(const BufferedReadersStorage&);

public:

    static BufferedReadersStorage * GetInstance() {
        static BufferedReadersStorage instance;
        return &instance;
    }


    std::vector< SequenceSingleReadStream* > * GetSingleReaders() const {
        return single_streams_;
    }

    std::vector< SequencePairedReadStream* > * GetPairedReaders() const {
        return paired_streams_;
    }

};


std::vector< SequenceSingleReadStream* > single_buffered_binary_readers(bool followed_by_rc, bool including_paired_reads) {
    BufferedReadersStorage * storage = BufferedReadersStorage::GetInstance();

    if (including_paired_reads) {
        return apply_single_wrappers(followed_by_rc, *(storage->GetSingleReaders()), storage->GetPairedReaders());
    }
    else {
        return apply_single_wrappers(followed_by_rc, *(storage->GetSingleReaders()));
    }
}

std::vector< SequencePairedReadStream* > paired_buffered_binary_readers(bool followed_by_rc, size_t insert_size) {
    BufferedReadersStorage * storage = BufferedReadersStorage::GetInstance();

    std::vector<SequencePairedReadStream*> paired_streams(cfg::get().thread_number);
    for (size_t i = 0; i < cfg::get().thread_number; ++i) {
        paired_streams[i] = new io::InsertSizeModifyingWrapper(*(storage->GetPairedReaders()->at(i)), insert_size);
    }
    return apply_paired_wrappers(followed_by_rc, paired_streams);
}

auto_ptr<SequenceSingleReadStream> single_buffered_binary_multireader(bool followed_by_rc, bool including_paired_reads) {
    return auto_ptr<SequenceSingleReadStream>(new io::MultifileReader<io::SingleReadSeq>(single_buffered_binary_readers(followed_by_rc, including_paired_reads)));
}

auto_ptr<SequencePairedReadStream> paired_buffered_binary_multireader(bool followed_by_rc, size_t insert_size) {
    return auto_ptr<SequencePairedReadStream>(new io::MultifileReader<io::PairedReadSeq>(paired_buffered_binary_readers(followed_by_rc, insert_size)));
}

}

#endif /* READ_CONVERTER_HPP_ */
