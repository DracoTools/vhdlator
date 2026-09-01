-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package cfg_pkg is
    type t_count is protected
        procedure bump (n : integer);
        impure function get return integer;
    end protected;
end package;

package body cfg_pkg is
    type t_count is protected body
        variable priv_n : integer := 0;
        procedure bump (n : integer) is
        begin
            priv_n := priv_n + n;
        end procedure;
        impure function get return integer is
        begin
            return priv_n;
        end function;
    end protected body;
end package body;

use work.cfg_pkg.all;

package obj_pkg is
    shared variable count : t_count;
end package;
