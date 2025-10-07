/*************************************************************************
 *
 * Copyright 2016 Realm Inc.
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
 **************************************************************************/

#include <iostream>


namespace realm {

struct Data {
    Data()
    {
        std::cout << "Data()\n";
    }
    ~Data()
    {
        std::cout << "~Data()\n";
    }
    Data* clone() const
    {
        return new Data();
    }
};


struct CopyAndMove {
    CopyAndMove()
        : m_data(new Data())
    {
    }
    ~CopyAndMove()
    {
        delete m_data;
    }

    CopyAndMove(const CopyAndMove& a)
        : m_data(a.m_data->clone())
    {
        std::cout << "Copy CopyAndMove (constructor)\n";
    }
    CopyAndMove& operator=(CopyAndMove a)
    {
        delete m_data;
        m_data = a.m_data;
        a.m_data = 0;
        std::cout << "Move CopyAndMove (assign)\n";
        return *this;
    }

    friend CopyAndMove move(CopyAndMove& a)
    {
        Data* d = a.m_data;
        a.m_data = 0;
        std::cout << "Move CopyAndMove (move)\n";
        return CopyAndMove(d);
    }

private:
    friend class ConstCopyAndMove;

    Data* m_data;

    CopyAndMove(Data* d)
        : m_data(d)
    {
    }
};


struct ConstCopyAndMove {
    ConstCopyAndMove()
        : m_data(new Data())
    {
    }
    ~ConstCopyAndMove()
    {
        delete m_data;
    }

    ConstCopyAndMove(const ConstCopyAndMove& a)
        : m_data(a.m_data->clone())
    {
        std::cout << "Copy ConstCopyAndMove (constructor)\n";
    }
    ConstCopyAndMove& operator=(ConstCopyAndMove a)
    {
        delete m_data;
        m_data = a.m_data;
        a.m_data = 0;
        std::cout << "Move ConstCopyAndMove (assign)\n";
        return *this;
    }

    ConstCopyAndMove(CopyAndMove a)
        : m_data(a.m_data)
    {
        a.m_data = 0;
        std::cout << "Move CopyAndMove to ConstCopyAndMove (constructor)\n";
    }
    ConstCopyAndMove& operator=(CopyAndMove a)
    {
        delete m_data;
        m_data = a.m_data;
        a.m_data = 0;
        std::cout << "Move CopyAndMove to ConstCopyAndMove (assign)\n";
        return *this;
    }

    friend ConstCopyAndMove move(ConstCopyAndMove& a)
    {
        const Data* d = a.m_data;
        a.m_data = 0;
        std::cout << "Move ConstCopyAndMove (move)\n";
        return ConstCopyAndMove(d);
    }

private:
    const Data* m_data;

    ConstCopyAndMove(const Data* d)
        : m_data(d)
    {
    }
};

} // namespace realm


realm::CopyAndMove func(realm::CopyAndMove a)
{
    return move(a);
}

int main()
{
    realm::CopyAndMove x1, x2;
    std::cout << "---A---\n";
    x2 = x1;
    std::cout << "---B---\n";
    x2 = move(x1);

    std::cout << "---0---\n";
    realm::CopyAndMove a1;
    std::cout << "---1---\n";
    realm::CopyAndMove a2 = func(func(func(func(a1)))); // One genuine copy, and 'a1' is left untouched
    std::cout << "---2---\n";
    realm::CopyAndMove a3 = func(func(func(func(move(a2))))); // Zero genuine copies, and 'a2' is left truncated
    std::cout << "---3---\n";
    const realm::CopyAndMove a4(a3); // Copy
    std::cout << "---4---\n";
    realm::CopyAndMove a5(a4); // Copy from const
    std::cout << "---5---\n";
    static_cast<void>(a5);

    realm::ConstCopyAndMove b1(a1); // One genuine copy
    std::cout << "---6---\n";
    realm::ConstCopyAndMove b2(move(a1)); // Zero genuine copies, and 'a1' is left truncated
    std::cout << "---7---\n";
    realm::ConstCopyAndMove b3(a4); // One genuine copy from const
    std::cout << "---8---\n";
    realm::ConstCopyAndMove b4(func(func(func(func(a3))))); // One genuine copy, and 'a3' is left untouched
    std::cout << "---9---\n";
    realm::ConstCopyAndMove b5(func(func(func(func(move(a3)))))); // Zero genuine copies, and 'a3' is left truncated
    static_cast<void>(b1);
    static_cast<void>(b2);
    static_cast<void>(b3);
    static_cast<void>(b4);
    static_cast<void>(b5);

    return 0;
}
