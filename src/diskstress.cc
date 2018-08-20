// vim: ts=4:expandtab:shiftwidth=4:softtabstop=4

/*
 * 
 * Copyright 2018 Virtual Interconnect, LLC
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 */

// Needed for timing reads and writes.
#include <time.h>
#include <limits>

// Needed for direct IO, which C++'s primitives don't support.
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// Needed for I/O error handling.
#include <system_error>

// Needed for output.
#include <iostream>
#include <string>
#include <iomanip>

#include <vector>

// Needed for memset
#include <cstring>

// Class for carrying blobs of memory around. There's a more C++ idiomatic way
// to do the alignments, but I don't want to muck with proper custom allocators
// right now in my immediate environment.
class Buffer {
    private:
        Buffer();

    public:
        class FailedAllocationException {
            protected:
                std::string m_why;
                explicit FailedAllocationException(const std::string& why) : m_why(why) {};
            public:
                operator std::string() const { return m_why; }
        };
        class FailedAllocationBadAlignmentException : FailedAllocationException {
            public:
                FailedAllocationBadAlignmentException() : FailedAllocationException("Bad alignment") {}
        };
        class FailedAllocationBadSizeException : FailedAllocationException {
            public:
                FailedAllocationBadSizeException() : FailedAllocationException("Bad size") {}
        };
        class FailedAllocationFailedAlignmentException : FailedAllocationException {
            public:
                FailedAllocationFailedAlignmentException() : FailedAllocationException("Failed alignment") {}
        };
    protected:
        void* m_pBase = nullptr;
        size_t m_sz = 0;
    public:
        explicit Buffer(size_t sz) : Buffer(getpagesize(), sz) {}
        explicit Buffer(size_t align, size_t sz) : m_sz(sz) {
            // At least as big as a pointer.
            if(!(align >= sizeof(void*))) {
                throw FailedAllocationBadAlignmentException();
            }
            // Power of 2
            if(!((align & (align - 1)) == 0)) {
                throw FailedAllocationBadAlignmentException();
            }

            if(0 == sz) {
                throw FailedAllocationBadSizeException();
            }
            
            int ret = posix_memalign(&m_pBase, align, sz);

            if(0 != ret) {
                throw FailedAllocationFailedAlignmentException();
            }
        }

        ~Buffer() {
            free(m_pBase);
        }

        size_t size() const { return m_sz; }

        void* buf() const { return m_pBase; }
};

// Class providing some operator overloads and additional functionality
// for the timespec class. Implemented as a template to force comparisons only
// between clock IDs of the same type. And why should we enforce that at
// runtime, when we can enforce it at compile time?
template<clockid_t clock_id> class Ts : public timespec {
    public:
        class OverflowException {
        };
        Ts() {
            // Initialize our timespec component
            int ret = clock_gettime(clock_id, this);

            if( 0 != ret) {
                throw std::system_error(errno, std::system_category());
            }
        }
    Ts operator-(const Ts& other) const {
        Ts ret;
        ret.tv_sec = this->tv_sec - other.tv_sec;
        ret.tv_nsec = this->tv_nsec - other.tv_nsec;

        // was other's nsec greater than our own?
        if(ret.tv_nsec < 0) {
            // Carry the subtraction.
            ret.tv_sec -= 1;
            ret.tv_nsec += 1000000000;
        }

        return ret;
    }

    Ts operator+(const Ts& other) const {
        Ts ret;

        if(std::numeric_limits<decltype(other.tv_sec)>::max() - other.tv_sec > this->tv_sec) {
            // We would overflow.
            throw OverflowException();
        }

        ret.tv_sec = this->tv_sec + other.tv_sec;
        ret.tv_nsec = this->tv_nsec + other.tv_nsec;

        if( ret.tv_nsec > 1000000000 ) {
            // Carry the addition.
            ret.tv_nsec -= 1000000000;
            ret.tv_sec += 1;
        }
        
        return ret;
    }

    // I'm not confident this entirely correct, but it's close enough for my
    // purposes.
    Ts operator/(const int& other) const {
        time_t intcomp = this->tv_sec / other;
        long secfraccomp = (1000000000 / other) * (this->tv_sec % other);

        Ts ret;
        ret.tv_sec = intcomp;
        ret.tv_nsec = this->tv_nsec + secfraccomp;
        return ret;
    }
};

typedef Ts<CLOCK_MONOTONIC> timing;

// Handy for formatting a timing.
std::ostream& operator<<(std::ostream& stream, const timespec& tm) {
   stream << tm.tv_sec << "." << std::setw(9) << std::setfill('0') << tm.tv_nsec;
   return stream;
}

// Handy for outputting a vector of timings as json.
template<typename t>
std::ostream& operator<<(std::ostream& stream, const std::vector<t>& timings) {
    stream << "[";
    if( timings.size() > 0 ) {
        typename std::vector<t>::const_iterator last = timings.end()-1;
        if( timings.size() > 1 ) {
            // Handle all but the last item, appending a ','.
            for(typename std::vector<t>::const_iterator tm = timings.begin(); tm != last; ++tm) {
                    stream << *tm << ",";
            }
        }

        // Emit the last item in the vector.
        stream << *last;
    }
    stream << "]";
    return stream;
}

// Simple class for encapsulating our file access.
class File {
    private:
        File() {}; // No bare construction.

    protected:
        int m_fd; // Pointer to FILE struct we wrap.

        // Wrap lseek() call. Called by a couple others.
        off_t _lseek(const off_t offset, const int whence) const {
            off_t ret = ::lseek(m_fd, offset, whence);

            if(-1 == ret) {
                throw std::system_error(errno, std::system_category());
            }

            return ret;
        }

    public:
        // Open the (existing; does not support O_CREATE) file ourselves.
        File(const std::string& pathname, const int flags) :
            m_fd(open(pathname.c_str(), flags)) {}

        // Standard destructor. Closes our fd. Raise std::system_error if
        // fclose() fails.
        ~File() {
            int ret = close(m_fd);

            if(0 != ret)
            {
                // Well, that sucks, but can you do?
            }
        }

        // Wrap lseek() call.
        off_t lseek(const off_t offset, const int whence) {
            return _lseek(offset, whence);
        }

        // Wrap write() call.
        ssize_t write(const Buffer& b) {
            ssize_t ret = ::write(m_fd, b.buf(), b.size());
            
            if(-1 == ret) {
                throw std::system_error(errno, std::system_category());
            }

            return ret;
        }

        // Wrap read() call.
        ssize_t read(Buffer& b) {
            ssize_t ret = ::read(m_fd, b.buf(), b.size());

            if(-1 == ret) {
                throw std::system_error(errno, std::system_category());
            }

            return ret;
        }

        // Helper function to get the size of the file.
        ssize_t size() const {
            // Get the current position.
            off_t cur_pos = _lseek(0, SEEK_CUR);

            // Get the size.
            off_t ret = _lseek(0, SEEK_END);

            // Return to the original position.
            _lseek(cur_pos, SEEK_SET);

            return ret;
        }
};


// Timed write call.
timing twrite(File& f, off_t offset, const Buffer& buf) {
    timing start;
    f.lseek(offset, SEEK_SET);
    f.write(buf);
    return timing() - start;
}

// Timed read call.
timing tread(File& f, off_t offset, Buffer& buf) {
    timing start;
    f.lseek(offset, SEEK_SET);
    f.read(buf);
    return timing() - start;
}

size_t getiosize(size_t minsize) {
    size_t pagesize = (size_t) getpagesize();
    if( minsize % getpagesize() == 0) {
        return minsize;
    }
    
    if( minsize < pagesize) {
        return minsize;
    }
    
    return (minsize / pagesize + 1) * pagesize;
}

void process_disk(const std::string& diskfile, size_t readsize, size_t writesize, off_t jumpincrement) {
    Buffer readbuf(readsize);
    Buffer writebuf(writesize);
    memset(writebuf.buf(), 0, writebuf.size());

    readsize = getiosize(readsize);
    writesize = getiosize(writesize);

    // Open the file object.
    File f(diskfile, O_RDWR | O_SYNC | O_DSYNC | O_DIRECT);

    ssize_t dsize = getiosize(f.size());

    timing acc_time;
    size_t tcount = 6;
    std::vector<timing> accumulator;
    size_t cycle_count = 0;

    // This is a butterfly-style test. Two interaction points, one starting at
    // the beginning of the disk, on starting and the end of the disk. After
    // each interaction cycle, the first point moves up one jump increment, and
    // the second point moves down one jump increment. At the beginning, we'll
    // beforcing the disk to seek as far as it can, which will force the read
    // head to travel long distances. The travel distance will shrink until the
    // two points meet. As the travel distance shrinks, we'll be forcing the
    // heads to change direction frequently and rapidly. I'm hoping for
    // mechanical strain, as well as possibly having a harmonic of the read
    // arm's resonant frequency interfere with track alignment. Though if I
    // really want to do the latter, I'd need to spend a lot of time focusing
    // on short seeks. Probably some interesting research to be found
    // monitoring the SMART data for spikes in ecc corrections at different
    // seek distances, but that's a project for a different time... 
    for (
            off_t begpos = 0, endpos = dsize - jumpincrement;
            (begpos < endpos)  &&
            ((ssize_t) (begpos + readsize) < dsize) &&
            (endpos > 0);
            begpos += jumpincrement, endpos -= jumpincrement
        )
    {
        timing cycle_start;
        std::vector<timing> round(tcount);
        round[0]=tread(f, begpos, readbuf);
        round[1]=tread(f, endpos, readbuf);
        round[2]=twrite(f, begpos, writebuf);
        round[3]=twrite(f, endpos, writebuf);
        round[4]=tread(f, begpos, readbuf);
        round[5]=tread(f, endpos, readbuf);

        std::vector<timing> new_accumulator = accumulator;
        bool bOverflowed = false;
        if(accumulator.size() > 0) {
            try {
                for(size_t tpos = 0; tpos < tcount; ++tpos) {
                    new_accumulator[tpos] = new_accumulator[tpos] + round[tpos];
                }
                accumulator = new_accumulator;
            } catch (const timing::OverflowException& e) {
                bOverflowed = true;
            }
        } else {
            accumulator = round;
        }

        if((cycle_start - acc_time).tv_sec >= 10) {
            std::vector<timing> avg(6);
            for(size_t tpos = 0; tpos < tcount; ++tpos) {
                timing posavg = accumulator[tpos] / cycle_count;
                avg[tpos] = posavg;
            }
            std::cout << diskfile << " " << cycle_count << " " << cycle_start - acc_time << " " << avg << " " << begpos << " " << endpos << " " << jumpincrement << std::endl << std::flush;
            // reset timer 
            acc_time = timing();
            if(bOverflowed) {
                // Accumulator overflowed. Reseed with this round.
                accumulator = round;
                cycle_count = 1;
            } else {
                accumulator.clear();
                cycle_count = 0;
            }

        } else
        {
            ++cycle_count;
        }
    }
}

int main(int argc, char** argv) {
    std::vector<std::string> args;
    args.reserve(argc);
    for(int argi = 0; argi < argc; argi++) {
        args.push_back(argv[argi]);
    }

    typedef std::vector<off_t> tcase_t;

    tcase_t readsizes = {1048576, 4096};
    tcase_t writesizes(readsizes);
    tcase_t increments = {1073741824,134217728,1048576,4096,1024};
    
    if( args.size() > 1 ) {
        try {
            for( tcase_t::iterator increment = increments.begin(); increment != increments.end(); ++increment) {
                for( tcase_t::iterator readsize = readsizes.begin(); readsize != readsizes.end(); ++readsize) {
                    for( tcase_t::iterator writesize = writesizes.begin(); writesize != writesizes.end(); ++writesize) {
                        process_disk(args[1], *readsize, *writesize, *increment);
                    }
                }
            }
        } catch (std::system_error& error) {
            std::cerr << "Error: " << error.code() << " - " << error.what() << std::endl;
        } catch (const Buffer::FailedAllocationException& ex) {
            std::cerr << "Error: " << (std::string) ex << std::endl;
        }
    } else {
        std::cerr << "Usage: " << args[0] << " (path to device/file)" << std::endl;
    }
}
