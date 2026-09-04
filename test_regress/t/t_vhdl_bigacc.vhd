-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package bigpk is
    -- "0 to integer'high / 2 - 1" is how a design spells "as long as
    -- whatever is on the other side made it": nothing ever allocates the
    -- whole of it, so it is a handle, not an array of a billion integers.
    type iv_t is array (natural range <>) of integer;
    type iv_acc_t is access iv_t;
    type big_acc_t is access iv_t (0 to integer'high / 2 - 1);
    type acc_vec_t is array (natural range <>) of iv_acc_t;
    type acc_vec_acc_t is access acc_vec_t;
    type big_vec_t is array (natural range <>) of big_acc_t;
    type big_vec_acc_t is access big_vec_t;

    type store_t is record
        n     : natural;
        ptrs  : acc_vec_acc_t;
        bigs  : big_vec_acc_t;
    end record;
end package;

entity t_vhdl_bigacc is
end entity;

use work.bigpk.all;

architecture tb of t_vhdl_bigacc is
begin
    process
        variable st : store_t := (0, null, null);
        variable sum : integer := 0;
    begin
        -- A handle nothing allocated is null, and stays comparable
        assert st.bigs = null report "bigs not null" severity failure;

        st.ptrs := new acc_vec_t'(0 to 3 => null);
        st.bigs := new big_vec_t'(0 to 3 => null);
        assert st.ptrs'length = 4 report "ptrs length" severity failure;
        assert st.bigs'length = 4 report "bigs length" severity failure;
        assert st.bigs(2) = null report "bigs(2) not null" severity failure;

        for i in 0 to 3 loop
            st.ptrs(i) := new iv_t'(0 to i => i + 1);
        end loop;
        for i in 0 to 3 loop
            sum := sum + st.ptrs(i)'length * st.ptrs(i)(0);
        end loop;
        assert sum = 30 report "sum = " & integer'image(sum) severity failure;

        for i in 0 to 3 loop
            deallocate(st.ptrs(i));
        end loop;
        deallocate(st.ptrs);
        deallocate(st.bigs);
        assert st.ptrs = null report "ptrs not deallocated" severity failure;

        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
