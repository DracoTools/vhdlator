-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package words is
    type t_word_array is array (natural range <>) of bit_vector;
    type t_frame is record
        data : t_word_array(0 to 1)(7 downto 0);
    end record;

    -- Two of these; which one a call means is settled by its arguments alone
    function widthof (v : bit_vector) return natural;
    function widthof (v : t_word_array) return natural;
end package;

package body words is
    function widthof (v : bit_vector) return natural is
    begin
        return v'length;
    end function;
    function widthof (v : t_word_array) return natural is
    begin
        return v'length * 8;
    end function;
end package body;

use work.words.all;

entity t_vhdl_selidx is
end entity;

architecture test of t_vhdl_selidx is

    -- "data" is also the name of the record's element, and inside here the
    -- parameter is what a simple name denotes -- but "f.data" is the
    -- element, whatever else is called that
    procedure store (constant data : in bit_vector; variable f : inout t_frame) is
    begin
        assert widthof(f.data) = 16
            report "f.data is " & integer'image(widthof(f.data)) severity failure;
        assert widthof(f.data(0)) = 8
            report "f.data(0) is " & integer'image(widthof(f.data(0))) severity failure;
        assert widthof(data) = 8 report "data is " & integer'image(widthof(data))
            severity failure;
        f.data(0) := data;
    end procedure;

begin
    check : process is
        variable f : t_frame;
    begin
        store(X"5A", f);
        assert f.data(0) = X"5A" report "got " & to_string(f.data(0)) severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
